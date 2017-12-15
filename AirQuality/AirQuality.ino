#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <SDS011.h>
#include "xCredentials.h"

#define DEVICE_TYPE "SDS011"

float pm10, pm10_10m_sum, pm10_24h_sum;
int pm10_10m_count, pm10_24h_count;
float pm25, pm25_10m_sum, pm25_24h_sum;
int pm25_10m_count, pm25_24h_count;
int error;

const char publishTopic[] = "iot-2/evt/status/fmt/json";

char server[] = ORG ".messaging.internetofthings.ibmcloud.com";
char authMethod[] = "use-token-auth";
char token[] = TOKEN;
char clientId[] = "d:" ORG ":" DEVICE_TYPE ":" DEVICE_ID;

WiFiClient wifiClient;
PubSubClient client(server, 1883, wifiClient);
SDS011 my_sds;

void setup() {
  my_sds.begin(D1, D2);
  Serial.begin(9600);

  wifiConnect();
  mqttConnect();

  Serial.println("Setup done.");
}

long lastPublishMillis = 0;
long last10mMillis = 0;
long last24hMillis = 0;
void loop() {
  long now = millis();

  error = my_sds.read(&pm25, &pm10);
  if (! error) {
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

    if (now - last10mMillis > 10 * 60 * 1000) {
      if (publish10mData()) {
        pm10_10m_sum = 0.0;
        pm10_10m_count = 0;
        pm25_10m_sum = 0.0;
        pm25_10m_count = 0;
      }
      last10mMillis = now;
    }

    if (now - last24hMillis > 24 * 60 * 60 * 1000) {
      if (publish24hData()) {
        pm10_24h_sum = 0.0;
        pm10_24h_count = 0;
        pm25_24h_sum = 0.0;
        pm25_24h_count = 0;
      }
      last24hMillis = now;
    }
  }

  if (!client.loop()) {
    mqttConnect();
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

  float pm10_10m_avg = pm10_10m_sum / pm10_10m_count;
  float pm25_10m_avg = pm25_10m_sum / pm25_10m_count;

  // construct a JSON response
  String json = "{\"d\":{";
  json += "\"PM25_10M\":";
  json += pm25_10m_avg;
  json += ",\"PM10_10M\":";
  json += pm10_10m_avg;
  json += "}}";

  if (client.publish(publishTopic, (char*) json.c_str())) {
    Serial.println("Publish 10m data OK");

    return true;
  } else {
    Serial.println("Publish 10m data FAILED");

    return false;
  }
}

boolean publish24hData() {
  float pm10_24h_avg = pm10_24h_sum / pm10_24h_count;
  float pm25_24h_avg = pm25_24h_sum / pm25_24h_count;

  // construct a JSON response
  String json = "{\"d\":{";
  json += "\"PM25_24H\":";
  json += pm25_24h_avg;
  json += ",\"PM10_24H\":";
  json += pm10_24h_avg;
  json += "}}";

  if (client.publish(publishTopic, (char*) json.c_str())) {
    Serial.println("Publish 24h data OK");

    return true;
  } else {
    Serial.println("Publish 24h data FAILED");

    return false;
  }
}

