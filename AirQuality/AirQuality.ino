#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <SDS011.h>
#include "xCredentials.h"

#define DEVICE_TYPE "SDS011"
#define JSON_BUFFER_LENGTH 100

float pm10, pm10_sum;
int pm10_count;
float pm25, pm25_sum;
int pm25_count;

const char publishTopic[] = "iot-2/evt/status/fmt/json";
char server[] = ORG ".messaging.internetofthings.ibmcloud.com";
char authMethod[] = "use-token-auth";
char token[] = TOKEN;
char clientId[] = "d:" ORG ":" DEVICE_TYPE ":" DEVICE_ID;

WiFiClient wifiClient;
PubSubClient client(server, 1883, wifiClient);
SDS011 sds;
const int led_pin = D3;

unsigned long lastAwake = 0;
unsigned long lastCheck = 0;
boolean isAwake = false;
boolean isReady = false;

void setup() {
  sds.begin(D1, D2);
  Serial.begin(9600);

  pinMode(led_pin, OUTPUT);
  digitalWrite(led_pin, LOW);

  wifiConnect();
  mqttConnect();

  sleepSds();
  Serial.println("Setup done.");
}

void loop() {
  unsigned long now = millis();

  if (!isAwake && (now - lastAwake > 8.5 * 60 * 1000)) {
      wakeupSds();
  }

  if (!isReady && (now - lastAwake > 9 * 60 * 1000)) {
    isReady = true;
    digitalWrite(led_pin, HIGH);
  }

  if (now - lastAwake > 10 * 60 * 1000) {
    lastAwake = now;
    isReady = false;
    sleepSds();
    digitalWrite(led_pin, LOW);

    if (!client.loop()) {
      mqttConnect();
    }

    if (publishData()) {
      pm10_sum = 0.0;
      pm10_count = 0;
      pm25_sum = 0.0;
      pm25_count = 0;
    }
  }

  if (isReady && (now - lastCheck > 2000)) {
    lastCheck = now;

    if (!sds.read(&pm25, &pm10)) {
      Serial.println("PM2.5: " + String(pm25));
      Serial.println("P10:  " + String(pm10));

      pm10_sum += pm10;
      pm10_count++;

      pm25_sum += pm25;
      pm25_count++;
    } else {
      Serial.println("Failed to read SDS.");
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
  WiFi.mode(WIFI_STA);
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

boolean publishData() {
  boolean ret = true;

  float pm10_avg = pm10_sum / pm10_count;
  float pm25_avg = pm25_sum / pm25_count;

  StaticJsonBuffer<JSON_BUFFER_LENGTH> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  JsonObject& d = root.createNestedObject("d");

  d["PM25"] = pm25_avg;
  d["PM10"] = pm10_avg;

  Serial.println("Publish payload:"); root.prettyPrintTo(Serial); Serial.println();

  char buff[JSON_BUFFER_LENGTH];
  root.printTo(buff, JSON_BUFFER_LENGTH);

  if (!client.publish(publishTopic, buff)) {
    Serial.println("Publish FAILED");
    ret = false;
  }

  return ret;
}

void sleepSds() {
  sds.sleep();
  isAwake = false;
  Serial.println("SDS011 is sleeping.");
}

void wakeupSds() {
  sds.wakeup();
  isAwake = true;
  Serial.println("SDS011 is awake.");
}


