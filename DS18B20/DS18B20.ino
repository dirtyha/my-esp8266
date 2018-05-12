#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "xCredentials.h"

#define ONE_WIRE_BUS D2
#define DHTTYPE DHT22   // there are multiple kinds of DHT sensors
#define DEVICE_TYPE "DS18B20"
#define JSON_BUFFER_LENGTH 100

const char publishTopic[] = "events/" DEVICE_TYPE "/" DEVICE_ID;
const char server[] = "myhomeat.cloud";
const char authMethod[] = "use-token-auth";
const char token[] = TOKEN;
const char clientId[] = "d:" DEVICE_TYPE ":" DEVICE_ID;

WiFiClient wifiClient;
PubSubClient client(server, 1883, wifiClient);
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensor(&oneWire);

void setup() {
  // default sleep 30 minutes
  unsigned long sleep = 1800e6;

  if (wifiConnect()) {
    if (mqttConnect()) {
      float ta = poll();
      if (ta > -30 && ta < 100) {
        publishData(ta);
        if (ta > 30) {
          // sleep only 2 minutes when ta up
          sleep = 120e6;
        }
      }
      mqttDisconnect();
    }
    wifiDisconnect();
  }

  ESP.deepSleep(sleep);
}

void loop() {
}

boolean wifiConnect() {
  WiFi.begin(ssid, password);
  int tryCount = 20;
  while (tryCount-- > 0 && WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  if (tryCount > 0) {
    WiFi.mode(WIFI_STA);
    return true;
  } else {
    return false;
  }
}

void wifiDisconnect() {
  if (WiFi.status() == WL_CONNECTED) {
    WiFi.disconnect();
  }
}

boolean mqttConnect() {
  int tryCount = 10;
  while (tryCount-- > 0 && !client.connect(clientId, authMethod, token)) {
    delay(500);
  }

  return tryCount > 0;
}

void mqttDisconnect() {
  if (client.connected()) {
    client.disconnect();
  }
}

void publishData(float ta) {
  StaticJsonBuffer<JSON_BUFFER_LENGTH> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  JsonObject& d = root.createNestedObject("d");

  d["TA"] = ta;

  char buff[JSON_BUFFER_LENGTH];
  root.printTo(buff, JSON_BUFFER_LENGTH);

  client.publish(publishTopic, buff);
}

float poll() {
  sensor.begin();
  sensor.requestTemperatures();

  return sensor.getTempCByIndex(0);
}


