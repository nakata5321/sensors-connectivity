#include "SdsDustSensor.h"
#include "secrets.h"    // use key generator to produce this file
#include <ESP8266WiFi.h>
#include <Ed25519.h>


#ifndef STASSID
#define STASSID "$$STASSID$$"
#define STAPSK  "$$STAPSK$$"
#endif

#define rxPin $$RXPIN$$     // D2 on ESP TX of the sensor is connected to RX of the board
#define txPin $$TXPIN$$     // D3 on ESP and vice versa sensor's rx is connected to boards's tx

const char* ssid     = STASSID;
const char* password = STAPSK;

const char* host = "$$HOST$$";
const uint16_t port = $$PORT$$;

const float GEO_LAT = $$GEOLAT$$;
const float GEO_LON = $$GEOLON$$;

const short model = 2;
const byte work_period = $$WORKPERIOD$$;     // minutes

uint8_t* privateKey = signing_key;
uint8_t* publicKey = verifying_key;

uint8_t buffer[16] = {0};
uint8_t signature[64] = {0};

SdsDustSensor sds(rxPin, txPin);
WiFiClient client;

void setup() {
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  sds.begin(9600);
  sds.setActiveReportingMode();
  sds.setCustomWorkingPeriod(work_period);

  if (!client.connect(host, port)) {
    Serial.println("connection failed");
    delay(5000);
    return;
  }

  if (client.connected()) {
    client.write(verifying_key, sizeof(verifying_key));
    client.write((byte*)&model, sizeof(model));
    client.flush();
  }
}

void loop() {
  PmResult pm = sds.readPm();

  if (!client.connected()) {
    client.connect(host, port);
    client.write(verifying_key, sizeof(verifying_key));
    client.write((byte*)&model, sizeof(model));
    client.flush();
  }

  if(pm.isOk()) {
    Serial.print("Got data: ");
    Serial.print(pm.pm25);
    Serial.print(", ");
    Serial.println(pm.pm10);
    memcpy(buffer, (byte*) &pm.pm25, 4);
    memcpy(buffer+4, (byte*) &pm.pm10, 4);
    memcpy(buffer+8, (byte*) &GEO_LAT, 4);
    memcpy(buffer+12, (byte*) &GEO_LON, 4);

    Ed25519::sign(signature, privateKey, publicKey, buffer, 16);
    client.write(buffer, sizeof(buffer));
    client.write(signature, sizeof(signature));
    client.flush();
   }

  delay(500);
}
