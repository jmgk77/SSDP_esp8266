// SSDP_esp8266.h
// based on https://github.com/esp8266/Arduino/issues/2283
// https://github.com/esp8266/Arduino/files/980894/SSDPDevice.zip
// by Pawel Dino

#include "SSDP_esp8266.h"
#include "lwip/igmp.h"

static const char *PROGMEM SSDP_RESPONSE_TEMPLATE =
	"HTTP/1.1 200 OK\r\n"
	"EXT:\r\n";

static const char *PROGMEM SSDP_NOTIFY_ALIVE_TEMPLATE =
	"NOTIFY * HTTP/1.1\r\n"
	"HOST: 239.255.255.250:1900\r\n"
	"NTS: ssdp:alive\r\n";

static const char *PROGMEM SSDP_NOTIFY_UPDATE_TEMPLATE =
	"NOTIFY * HTTP/1.1\r\n"
	"HOST: 239.255.255.250:1900\r\n"
	"NTS: ssdp:update\r\n";

static const char *PROGMEM SSDP_PACKET_TEMPLATE =
	"%s"									 // _ssdp_response_template / _ssdp_notify_template
	"CACHE-CONTROL: max-age=%u\r\n"			 // SSDP_INTERVAL
	"SERVER: UPNP/1.1 %s/%s\r\n"			 // m_modelName, m_modelNumber
	"USN: %s%s%s\r\n"						 // m_uuid
	"%s: %s\r\n"							 // "NT" or "ST", m_deviceType
	"LOCATION: http://%u.%u.%u.%u:%u/%s\r\n" // WiFi.localIP(), m_port, m_schemaURL
	"\r\n";

static const char *PROGMEM SSDP_SCHEMA_TEMPLATE_HEADER =
	"HTTP/1.1 200 OK\r\n"
	"Content-Type: text/xml\r\n"
	"Connection: close\r\n"
	"Access-Control-Allow-Origin: *\r\n"
	"\r\n";

static const char *PROGMEM SSDP_SCHEMA_TEMPLATE =
	"%s"
	"<?xml version=\"1.0\"?>"
	"<root xmlns=\"urn:schemas-upnp-org:device-1-0\">"
	"<specVersion>"
	"<major>1</major>"
	"<minor>0</minor>"
	"</specVersion>"
	"<URLBase>http://%u.%u.%u.%u:%u/%s</URLBase>" // WiFi.localIP(), _port
	"<device>"
	"<deviceType>%s</deviceType>"
	"<friendlyName>%s</friendlyName>"
	"<presentationURL>%s</presentationURL>"
	"<serialNumber>%s</serialNumber>"
	"<modelName>%s</modelName>"
	"<modelNumber>%s</modelNumber>"
	"<modelURL>%s</modelURL>"
	"<manufacturer>%s</manufacturer>"
	"<manufacturerURL>%s</manufacturerURL>"
	"<UDN>uuid:%s</UDN>"
	"</device>"
	//    "<iconList>"
	//      "<icon>"
	//        "<mimetype>image/png</mimetype>"
	//        "<height>48</height>"
	//        "<width>48</width>"
	//        "<depth>24</depth>"
	//        "<url>icon48.png</url>"
	//      "</icon>"
	//      "<icon>"
	//       "<mimetype>image/png</mimetype>"
	//       "<height>120</height>"
	//       "<width>120</width>"
	//       "<depth>24</depth>"
	//       "<url>icon120.png</url>"
	//      "</icon>"
	//    "</iconList>"
	"</root>\r\n"
	"\r\n";

SSDP_esp8266Class::SSDP_esp8266Class() : m_server(0),
										 m_port(80),
										 m_ttl(SSDP_MULTICAST_TTL)
{
	m_uuid[0] = '\0';
	m_modelNumber[0] = '\0';
	sprintf(m_deviceType, "urn:schemas-upnp-org:device:Basic:1");
	m_friendlyName[0] = '\0';
	m_presentationURL[0] = '\0';
	m_serialNumber[0] = '\0';
	m_modelName[0] = '\0';
	m_modelURL[0] = '\0';
	m_manufacturer[0] = '\0';
	m_manufacturerURL[0] = '\0';
	sprintf(m_schemaURL, "ssdp/schema.xml");

#ifdef ESP8266
	uint32_t chipId = ESP.getChipId();
#endif
#ifdef ESP32
	uint32_t chipId = ESP.getEfuseMac();
#endif

	sprintf(m_uuid, "38323636-4558-4dda-9188-cda0e6%02x%02x%02x",
			(uint16_t)((chipId >> 16) & 0xff),
			(uint16_t)((chipId >> 8) & 0xff),
			(uint16_t)chipId & 0xff);

	for (int i = 0; i < SSDP_QUEUE_SIZE; i++)
	{
		m_queue[i].time = 0;
	}
}

void SSDP_esp8266Class::update()
{
	postNotifyUpdate();
}

bool SSDP_esp8266Class::readLine(String &value)
{
	char buffer[65];
	int bufferPos = 0;

	while (1)
	{
		int c = m_server->read();

		if (c < 0)
		{
			buffer[bufferPos] = '\0';

			break;
		}
		if (c == '\r' && m_server->peek() == '\n')
		{
			m_server->read();

			buffer[bufferPos] = '\0';

			break;
		}
		if (bufferPos < 64)
		{
			buffer[bufferPos++] = c;
		}
	}

	value = String(buffer);

	return bufferPos > 0;
}

bool SSDP_esp8266Class::readKeyValue(String &key, String &value)
{
	char buffer[65];
	int bufferPos = 0;

	while (1)
	{
		int c = m_server->read();

		if (c < 0)
		{
			if (bufferPos == 0)
				return false;

			buffer[bufferPos] = '\0';

			break;
		}
		if (c == ':')
		{
			buffer[bufferPos] = '\0';

			while (m_server->peek() == ' ')
				m_server->read();

			break;
		}
		else if (c == '\r' && m_server->peek() == '\n')
		{
			m_server->read();

			if (bufferPos == 0)
				return false;

			buffer[bufferPos] = '\0';

			key = String();
			value = String(buffer);

			return true;
		}
		if (bufferPos < 64)
		{
			buffer[bufferPos++] = c;
		}
	}

	key = String(buffer);

	readLine(value);

	return true;
}

void SSDP_esp8266Class::postNotifyALive()
{
	unsigned long time = millis();

	post(NOTIFY_ALIVE_INIT, ROOT_FOR_ALL, SSDP_MULTICAST_ADDR, SSDP_PORT, time + 10);
	post(NOTIFY_ALIVE_INIT, ROOT_BY_UUID, SSDP_MULTICAST_ADDR, SSDP_PORT, time + 55);
	post(NOTIFY_ALIVE_INIT, ROOT_BY_TYPE, SSDP_MULTICAST_ADDR, SSDP_PORT, time + 80);

	post(NOTIFY_ALIVE_INIT, ROOT_FOR_ALL, SSDP_MULTICAST_ADDR, SSDP_PORT, time + 210);
	post(NOTIFY_ALIVE_INIT, ROOT_BY_UUID, SSDP_MULTICAST_ADDR, SSDP_PORT, time + 255);
	post(NOTIFY_ALIVE_INIT, ROOT_BY_TYPE, SSDP_MULTICAST_ADDR, SSDP_PORT, time + 280);

	post(NOTIFY_ALIVE, ROOT_FOR_ALL, SSDP_MULTICAST_ADDR, SSDP_PORT, time + 610);
	post(NOTIFY_ALIVE, ROOT_BY_UUID, SSDP_MULTICAST_ADDR, SSDP_PORT, time + 655);
	post(NOTIFY_ALIVE, ROOT_BY_TYPE, SSDP_MULTICAST_ADDR, SSDP_PORT, time + 680);
}

void SSDP_esp8266Class::postNotifyUpdate()
{
	unsigned long time = millis();

	post(NOTIFY_UPDATE, ROOT_FOR_ALL, SSDP_MULTICAST_ADDR, SSDP_PORT, time + 10);
	post(NOTIFY_UPDATE, ROOT_BY_UUID, SSDP_MULTICAST_ADDR, SSDP_PORT, time + 55);
	post(NOTIFY_UPDATE, ROOT_BY_TYPE, SSDP_MULTICAST_ADDR, SSDP_PORT, time + 80);
}

void SSDP_esp8266Class::postResponse(long mx)
{
	unsigned long time = millis();
	unsigned long delay = random(0, mx) * 900L; // 1000 ms - 100 ms

	IPAddress address = m_server->remoteIP();
	uint16_t port = m_server->remotePort();

	post(RESPONSE, ROOT_FOR_ALL, address, port, time + delay / 3);
	post(RESPONSE, ROOT_BY_UUID, address, port, time + delay / 3 * 2);
	post(RESPONSE, ROOT_BY_TYPE, address, port, time + delay);
}

void SSDP_esp8266Class::postResponse(ssdp_udn_t udn, long mx)
{
	post(RESPONSE, udn, m_server->remoteIP(), m_server->remotePort(), millis() + random(0, mx) * 900L); // 1000 ms - 100 ms
}

void SSDP_esp8266Class::post(ssdp_message_t type, ssdp_udn_t udn, IPAddress address, uint16_t port, unsigned long time)
{
	for (int i = 0; i < SSDP_QUEUE_SIZE; i++)
	{
		if (m_queue[i].time == 0)
		{
			m_queue[i].type = type;
			m_queue[i].udn = udn;
			m_queue[i].address = address;
			m_queue[i].port = port;
			m_queue[i].time = time;

			break;
		}
	}
}

void SSDP_esp8266Class::send(ssdp_send_parameters_t *parameters)
{
	uint8_t buffer[1460];
	unsigned int ip = WiFi.localIP();

	const char *typeTemplate;
	const char *uri = nullptr, *usn1 = nullptr, *usn2 = nullptr, *usn3 = nullptr;

	switch (parameters->type)
	{
	case NOTIFY_ALIVE_INIT:
	case NOTIFY_ALIVE:
		typeTemplate = SSDP_NOTIFY_ALIVE_TEMPLATE;
		break;
	case NOTIFY_UPDATE:
		typeTemplate = SSDP_NOTIFY_UPDATE_TEMPLATE;
		break;
	default: // RESPONSE
		typeTemplate = SSDP_RESPONSE_TEMPLATE;
		break;
	}

	String uuid = "uuid:" + String(m_uuid);

	switch (parameters->udn)
	{
	case ROOT_FOR_ALL:
		uri = "upnp:rootdevice";
		usn1 = uuid.c_str();
		usn2 = "::";
		usn3 = "upnp:rootdevice";
		break;
	case ROOT_BY_UUID:
		uri = uuid.c_str();
		usn1 = uuid.c_str();
		usn2 = "";
		usn3 = "";
		break;
	case ROOT_BY_TYPE:
		uri = m_deviceType;
		usn1 = uuid.c_str();
		usn2 = "::";
		usn3 = m_deviceType;
		break;
	}

	int len = snprintf_P((char *)buffer, sizeof(buffer),
						 SSDP_PACKET_TEMPLATE, typeTemplate,
						 SSDP_INTERVAL, m_modelName, m_modelNumber, usn1, usn2, usn3, parameters->type == RESPONSE ? "ST" : "NT", uri,
						 LIP2STR(&ip), m_port, m_schemaURL);

	if (parameters->address == (uint32_t)SSDP_MULTICAST_ADDR)
	{
#ifdef ESP8266
		m_server->beginPacketMulticast(parameters->address, parameters->port, m_ttl);
#endif
#ifdef ESP32
		m_server->beginMulticast(parameters->address, parameters->port);
		m_server->beginMulticastPacket();
#endif
	}
	else
	{
		m_server->beginPacket(parameters->address, parameters->port);
	}

	m_server->write(buffer, len);
	m_server->endPacket();

	parameters->time = parameters->type == NOTIFY_ALIVE ? parameters->time + SSDP_INTERVAL * 900L : 0; // 1000 ms - 100 ms
}

void SSDP_esp8266Class::schema(WiFiClient client)
{
	uint32_t ip = WiFi.localIP();
	client.printf(SSDP_SCHEMA_TEMPLATE, SSDP_SCHEMA_TEMPLATE_HEADER,
				  LIP2STR(&ip), m_port, m_schemaURL,
				  m_deviceType,
				  m_friendlyName,
				  m_presentationURL,
				  m_serialNumber,
				  m_modelName,
				  m_modelNumber,
				  m_modelURL,
				  m_manufacturer,
				  m_manufacturerURL,
				  m_uuid);
}

String SSDP_esp8266Class::schema()
{
	uint32_t ip = WiFi.localIP();
	char buf[sizeof(SSDP_SCHEMA_TEMPLATE) + sizeof(m_port) + sizeof(m_schemaURL) + sizeof(m_deviceType) +
			 sizeof(m_friendlyName) + sizeof(m_presentationURL) + sizeof(m_serialNumber) + sizeof(m_modelName) +
			 sizeof(m_modelNumber) + sizeof(m_modelURL) + sizeof(m_manufacturer) + sizeof(m_manufacturerURL) +
			 sizeof(m_uuid) + 256];
	sprintf(buf, SSDP_SCHEMA_TEMPLATE, "",
			LIP2STR(&ip), m_port, m_schemaURL,
			m_deviceType,
			m_friendlyName,
			m_presentationURL,
			m_serialNumber,
			m_modelName,
			m_modelNumber,
			m_modelURL,
			m_manufacturer,
			m_manufacturerURL,
			m_uuid);
	return String(buf);
}

void SSDP_esp8266Class::handleClient()
{
	IPAddress current = WiFi.localIP();

	if (m_last != current)
	{
		m_last = current;

		for (int i = 0; i < SSDP_QUEUE_SIZE; i++)
		{
			m_queue[i].time = 0;
		}

		if (current != INADDR_NONE)
		{
			if (!m_server)
				m_server = new WiFiUDP();

#ifdef ESP8266
			m_server->beginMulticast(current, SSDP_MULTICAST_ADDR, SSDP_PORT);
#endif
#ifdef ESP32
			m_server->beginMulticast(SSDP_MULTICAST_ADDR, SSDP_PORT);
			m_server->beginMulticastPacket();
#endif
			postNotifyALive();
		}
		else if (m_server)
		{
			m_server->stop();
		}
	}

	if (m_server && m_server->parsePacket())
	{
		String value;

		if (readLine(value) && value.equalsIgnoreCase("M-SEARCH * HTTP/1.1"))
		{
			String key, st;
			bool host = false, man = false;
			long mx = 0;

			while (readKeyValue(key, value))
			{
				if (key.equalsIgnoreCase("HOST") && value.equals("239.255.255.250:1900"))
				{
					host = true;
				}
				else if (key.equalsIgnoreCase("MAN") && value.equals("\"ssdp:discover\""))
				{
					man = true;
				}
				else if (key.equalsIgnoreCase("ST"))
				{
					st = value;
				}
				else if (key.equalsIgnoreCase("MX"))
				{
					mx = value.toInt();
				}
			}

			if (host && man && mx > 0)
			{
				if (st.equals("ssdp:all"))
				{
					postResponse(mx);
				}
				else if (st.equals("upnp:rootdevice"))
				{
					postResponse(ROOT_FOR_ALL, mx);
				}
				else if (st.equals("uuid:" + String(m_uuid)))
				{
					postResponse(ROOT_BY_UUID, mx);
				}
				else if (st.equals(m_deviceType))
				{
					postResponse(ROOT_BY_TYPE, mx);
				}
			}
		}

		m_server->flush();
	}
	else
	{
		unsigned long time = millis();

		for (int i = 0; i < SSDP_QUEUE_SIZE; i++)
		{
			if (m_queue[i].time > 0 && m_queue[i].time < time)
			{
				send(&m_queue[i]);
			}
		}
	}
}

void SSDP_esp8266Class::setSchemaURL(const char *url)
{
	strlcpy(m_schemaURL, url, sizeof(m_schemaURL));
}

void SSDP_esp8266Class::setHTTPPort(uint16_t port)
{
	m_port = port;
}

void SSDP_esp8266Class::setDeviceType(const char *deviceType)
{
	strlcpy(m_deviceType, deviceType, sizeof(m_deviceType));
}

void SSDP_esp8266Class::setName(const char *name)
{
	strlcpy(m_friendlyName, name, sizeof(m_friendlyName));
}

void SSDP_esp8266Class::setURL(const char *url)
{
	strlcpy(m_presentationURL, url, sizeof(m_presentationURL));
}

void SSDP_esp8266Class::setSerialNumber(const char *serialNumber)
{
	strlcpy(m_serialNumber, serialNumber, sizeof(m_serialNumber));
}

void SSDP_esp8266Class::setSerialNumber(const uint32_t serialNumber)
{
	snprintf(m_serialNumber, sizeof(uint32_t) * 2 + 1, "%08X", serialNumber);
}

void SSDP_esp8266Class::setModelName(const char *name)
{
	strlcpy(m_modelName, name, sizeof(m_modelName));
}

void SSDP_esp8266Class::setModelNumber(const char *num)
{
	strlcpy(m_modelNumber, num, sizeof(m_modelNumber));
}

void SSDP_esp8266Class::setModelURL(const char *url)
{
	strlcpy(m_modelURL, url, sizeof(m_modelURL));
}

void SSDP_esp8266Class::setManufacturer(const char *name)
{
	strlcpy(m_manufacturer, name, sizeof(m_manufacturer));
}

void SSDP_esp8266Class::setManufacturerURL(const char *url)
{
	strlcpy(m_manufacturerURL, url, sizeof(m_manufacturerURL));
}

void SSDP_esp8266Class::setTTL(const uint8_t ttl)
{
	m_ttl = ttl;
}

SSDP_esp8266Class SSDP_esp8266;
