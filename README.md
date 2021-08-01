# SSDP_esp8266

Support SSDP for ESP8266/ESP32

based on https://github.com/esp8266/Arduino/issues/2283 (https://github.com/esp8266/Arduino/files/980894/SSDPDevice.zip) by Pawel Dino

forked from https://github.com/gitpeut/esp32SSDP

Minor adjustments and library-fy by jmgk77

## Usage

1. Include "SSDP_esp8266.h"
2. Call several SSDP_esp8266.setXXX() with your data
3. Call SSDP_esp8266.setURL() with your main URL (index.html or similar)
4. Call SSDP_esp8266.setSchemaURL() with the URL where you want to serve your XML schema
5. Set a www handler for this URL where you send SSDP_esp8266.schema() return
6. Call SSDP_esp8266.handleClient() in your main loop

## Tests

Work with esp8266 (nodemcu v1.1) with Arduino IDE and with VSCode + platform.io

Not tested with ESP32, but should work.
