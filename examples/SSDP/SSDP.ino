#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

#include "SSDPDevice.h"

#define STASSID "SSID"
#define STAPSK  "PSW"

const char* ssid = STASSID;
const char* password = STAPSK;

ESP8266WebServer HTTP(80);

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("Starting WiFi...");

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  if (WiFi.waitForConnectResult() == WL_CONNECTED) {

    Serial.printf("Starting HTTP...\n");
    HTTP.on("/index.html", HTTP_GET, []() {
      HTTP.send(200, "text/plain", "Hello World!");
    });
    HTTP.on("/description.xml", HTTP_GET, []() {
      SSDPDevice.schema(HTTP.client());
    });
    HTTP.begin();

    Serial.printf("Starting SSDP...\n");

    SSDPDevice.setName( "Philips hue clone" );
    SSDPDevice.setDeviceType("urn:schemas-upnp-org:device:mydevice:1");
    SSDPDevice.setSchemaURL("description.xml");
    SSDPDevice.setSerialNumber("001788102201");
    SSDPDevice.setURL("index.html");
    SSDPDevice.setModelName("Philips hue bridge 2012");
    SSDPDevice.setModelNumber("929000226503");
    SSDPDevice.setManufacturer("Royal Philips Electronics");
    SSDPDevice.setManufacturerURL("http://www.philips.com");

    Serial.printf("Ready!\n");
  } else {
    Serial.printf("WiFi Failed\n");
    while (1) {
      delay(100);
    }
  }
}

void loop() {
  HTTP.handleClient();
  SSDPDevice.handleClient();

  delay(1);
}
