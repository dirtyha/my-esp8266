#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <SDS011.h>
#include "xCredentials.h"

#define DEVICE_TYPE "SDS011"
#define JSON_BUFFER_LENGTH 100

float pm10, pm10_10m_sum, pm10_24h_sum;
int pm10_10m_count, pm10_24h_count;
float pm25, pm25_10m_sum, pm25_24h_sum;
int pm25_10m_count, pm25_24h_count;

const char publishTopic[] = "iot-2/evt/status/fmt/json";
char server[] = ORG ".messaging.internetofthings.ibmcloud.com";
char authMethod[] = "use-token-auth";
char token[] = TOKEN;
char clientId[] = "d:" ORG ":" DEVICE_TYPE ":" DEVICE_ID;

WiFiClient wifiClient;
PubSubClient client(server, 1883, wifiClient);
SDS011 sds;

unsigned long lastPublish = 0;
unsigned long last10mCheck = 0;
unsigned long last24hCheck = 0;

void setup() {
  sds.begin(D1, D2);
  Serial.begin(9600);

  wifiConnect();
  mqttConnect();

  Serial.println("Setup done.");
}

void loop() {
  unsigned long now = millis();

  if (!sds.read(&pm25, &pm10)) {
    //    Serial.println("PM2.5: " + String(pm25));
    //    Serial.println("P10:  " + String(pm10));

    pm10_10m_sum += pm10;
    pm10_10m_count++;
    pm10_24h_sum += pm10;
    pm10_24h_count++;

    pm25_10m_sum += pm25;
    pm25_10m_count++;
    pm25_24h_sum += pm25;
    pm25_24h_count++;

    if (!client.loop()) {
      mqttConnect();
    }

    if (now - last10mCheck > 10 * 60 * 1000) {
      last10mCheck = now;
      if (publish10mData()) {
        pm10_10m_sum = 0.0;
        pm10_10m_count = 0;
        pm25_10m_sum = 0.0;
        pm25_10m_count = 0;
      }
    }

    if (now - last24hCheck > 24 * 60 * 60 * 1000) {
      last24hCheck = now;
      if (publish24hData()) {
        pm10_24h_sum = 0.0;
        pm10_24h_count = 0;
        pm25_24h_sum = 0.0;
        pm25_24h_count = 0;
      }
    }
  }
}

void wifiConnect() {
  Serial.print("Connecting to "); Serial.print(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.print("WiFi connected, IP address: "); Serial.println(WiFi.localIP());
}

void mqttConnect() {
  if (!!!client.connected()) {
    Serial.print("Reconnecting MQTT client to "); Serial.println(server);
    int tryCount = 60;
    while (!!!client.connect(clientId, authMethod, token) && --tryCount > 0) {
      Serial.print(".");
      delay(500);
    }
    Serial.println();
  }
}

boolean publish10mData() {
  boolean ret = true;

  float pm10_10m_avg = pm10_10m_sum / pm10_10m_count;
  float pm25_10m_avg = pm25_10m_sum / pm25_10m_count;

  StaticJsonBuffer<JSON_BUFFER_LENGTH> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  JsonObject& d = root.createNestedObject("d");

  d["PM25_10M"] = pm25_10m_avg;
  d["PM10_10M"] = pm10_10m_avg;

  //  Serial.println("Publish payload:"); root.prettyPrintTo(Serial); Serial.println();

  char buff[JSON_BUFFER_LENGTH];
  root.printTo(buff, JSON_BUFFER_LENGTH);

  if (!client.publish(publishTopic, buff)) {
    Serial.println("Publish FAILED");
    ret = false;
  }

  return ret;
}

boolean publish24hData() {
  boolean ret = true;
  
  float pm10_24h_avg = pm10_24h_sum / pm10_24h_count;
  float pm25_24h_avg = pm25_24h_sum / pm25_24h_count;

  StaticJsonBuffer<JSON_BUFFER_LENGTH> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  JsonObject& d = root.createNestedObject("d");

  d["PM25_24H"] = pm25_24h_avg;
  d["PM10_24H"] = pm10_24h_avg;

  //  Serial.println("Publish payload:"); root.prettyPrintTo(Serial); Serial.println();

  char buff[JSON_BUFFER_LENGTH];
  root.printTo(buff, JSON_BUFFER_LENGTH);

  if (!client.publish(publishTopic, buff)) {
    Serial.println("Publish FAILED");
    ret = false;
  }

  return ret;
}

