#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <LaCrosse_TX23.h>
#include <DHT.h>
#include "xCredentials.h"

#define DHTPIN D3     // what digital pin the DHT22 is conected to
#define DHTTYPE DHT22   // there are multiple kinds of DHT sensors
#define JSON_BUFFER_LENGTH 150
#define DEVICE_TYPE "WeatherStation"

char server[] = "myhomeat.cloud";
char authMethod[] = "use-token-auth";
char token[] = TOKEN;
char clientId[] = "d:" DEVICE_TYPE ":" DEVICE_ID;
const char publishTopic[] = "events/" DEVICE_TYPE "/" DEVICE_ID;

WiFiClient wifiClient;
PubSubClient client(server, 1883, wifiClient);
LaCrosse_TX23 tx23 = LaCrosse_TX23(D2);
DHT dht(DHTPIN, DHTTYPE);
unsigned long lastRead = 0;
unsigned long lastSent = 0;

void setup() {
  // initialize serial communication with computer:
  Serial.begin(57600);
  while (!Serial) {}

  wifiConnect();
  mqttConnect();

  Serial.println("Setup done.");
}

void loop() {
  unsigned long now = millis();

  if (now - lastSent > 60000) {
    lastSent = now;

    Serial.println("Measuring...");

    float ta, td = NAN, rh;
    int tryCount = 10;
    while (tryCount-- > 0) {
      rh = dht.readHumidity();
      ta = dht.readTemperature();

      if (isnan(rh) || isnan(ta)) {
        Serial.println("Failed to read DHT22.");
        delay(2000);
      } else {
        td = calculateTd(rh, ta);
        tryCount = 0;
      }
    }

    float ws, f_wd;
    int i_wd;
    bool isOk = tx23.read(ws, i_wd);
    if (isOk) {
      f_wd = 22.5 * i_wd;
    } else {
      Serial.println("Failed to read TX23.");
      ws = NAN;
      f_wd = NAN;
    }

    // reconnect and send data
    if (!client.loop()) {
      mqttConnect();
    }
    publishData(ws, f_wd, ta, rh, td);
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
    int retryCount = 60;
    while (!!!client.connect(clientId, authMethod, token) && --retryCount > 0) {
      Serial.print(".");
      delay(500);
    }
    Serial.println();
  }
}

void publishData(float ws, float wd, float ta, float rh, float td) {
  StaticJsonBuffer<JSON_BUFFER_LENGTH> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  JsonObject& d = root.createNestedObject("d");

  if (!isnan(ta)) {
    d["TA"] = ta;
  }
  if (!isnan(rh)) {
    d["RH"] = rh;
  }
  if (!isnan(td)) {
    d["Td"] = td;
  }
  if (!isnan(ws)) {
    d["WS"] = ws;
  }
  if (!isnan(wd)) {
    d["WD"] = wd;
  }

  char buff[JSON_BUFFER_LENGTH];
  root.printTo(buff, JSON_BUFFER_LENGTH);
  Serial.print("Message = ");Serial.println(buff);

  if(client.publish(publishTopic, buff)) {
    Serial.println("Publish ok.");
  } else {
    Serial.println("Publish failed.");
  }
}

float calculateTd(float rh, float ta) {
  return 243.04 * (log(rh / 100.0) + ((17.625 * ta) / (243.04 + ta))) / (17.625 - log(rh / 100.0) - ((17.625 * ta) / (243.04 + ta)));
}

