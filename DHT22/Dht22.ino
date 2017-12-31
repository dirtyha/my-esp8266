#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include "xCredentials.h"

#define DHTPIN 4     // what digital pin the DHT22 is conected to
#define DHTTYPE DHT22   // there are multiple kinds of DHT sensors
#define DEVICE_TYPE "DHT22"
#define JSON_BUFFER_LENGTH 100

const char publishTopic[] = "iot-2/evt/status/fmt/json";
const char server[] = ORG ".messaging.internetofthings.ibmcloud.com";
const char authMethod[] = "use-token-auth";
const char token[] = TOKEN;
const char clientId[] = "d:" ORG ":" DEVICE_TYPE ":" DEVICE_ID;

WiFiClient wifiClient;
PubSubClient client(server, 1883, wifiClient);
DHT dht(DHTPIN, DHTTYPE);

typedef struct {
  float ta;
  float td;
  float rh;
} dht_result;
dht_result result;

void setup() {
  if (wifiConnect()) {
    if (mqttConnect()) {
      publishData();
      mqttDisconnect();
    }
    wifiDisconnect();
  }

  // sleep 10 minutes
  ESP.deepSleep(600e6);
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

void publishData() {
  dht_result* r = poll();
  if (r != NULL) {
    StaticJsonBuffer<JSON_BUFFER_LENGTH> jsonBuffer;
    JsonObject& root = jsonBuffer.createObject();
    JsonObject& d = root.createNestedObject("d");

    d["TA"] = r->ta;
    d["Td"] = r->td;
    d["RH"] = r->rh;

    char buff[JSON_BUFFER_LENGTH];
    root.printTo(buff, JSON_BUFFER_LENGTH);

    client.publish(publishTopic, buff);
  }
}

dht_result* poll() {
  int tryCount = 10;
  while (tryCount-- > 0) {
    float rh = dht.readHumidity();
    float ta = dht.readTemperature();

    if (isnan(rh) || isnan(ta)) {
      delay(2000);
    } else {
      result.rh = rh;
      result.ta = ta;
      result.td = calculateTd(rh, ta);

      return &result;
    }
  }

  return NULL;
}

float calculateTd(float rh, float ta) {
  return 243.04 * (log(rh / 100.0) + ((17.625 * ta) / (243.04 + ta))) / (17.625 - log(rh / 100.0) - ((17.625 * ta) / (243.04 + ta)));
}

