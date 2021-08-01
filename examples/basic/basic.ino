// SSDP_basic example
// based on https://github.com/esp8266/Arduino/issues/2283
// https://github.com/esp8266/Arduino/files/980894/SSDPDevice.zip
// by Pawel Dino

#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#endif

#ifdef ESP32
#include <WiFi.h>
#include <WiFiUdp.h>
#endif

#include "SSDP_esp8266.h"

#define STASSID "SSID"
#define STAPSK "PSW"

const char *ssid = STASSID;
const char *password = STAPSK;

ESP8266WebServer HTTP(80);

void setup()
{
  Serial.begin(115200);
  Serial.println();
  Serial.println("Starting WiFi...");

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  if (WiFi.waitForConnectResult() == WL_CONNECTED)
  {

    Serial.printf("Starting HTTP...\n");
    HTTP.on("/index.html", HTTP_GET, []()
            { HTTP.send(200, "text/plain", "Hello World!"); });
    HTTP.on("/description.xml", HTTP_GET, []()
            { SSDP_esp8266.schema(HTTP.client()); });
    HTTP.begin();

    Serial.printf("Starting SSDP...\n");

    SSDP_esp8266.setName("Philips hue clone");
    SSDP_esp8266.setDeviceType("urn:schemas-upnp-org:device:mydevice:1");
    SSDP_esp8266.setSchemaURL("description.xml");
    SSDP_esp8266.setSerialNumber("001788102201");
    SSDP_esp8266.setURL("index.html");
    SSDP_esp8266.setModelName("Philips hue bridge 2012");
    SSDP_esp8266.setModelNumber("929000226503");
    SSDP_esp8266.setManufacturer("Royal Philips Electronics");
    SSDP_esp8266.setManufacturerURL("http://www.philips.com");

    Serial.printf("Ready!\n");
  }
  else
  {
    Serial.printf("WiFi Failed\n");
    while (1)
    {
      delay(100);
    }
  }
}

void loop()
{
  HTTP.handleClient();
  SSDP_esp8266.handleClient();

  delay(1);
}
