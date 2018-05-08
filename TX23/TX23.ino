#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <LaCrosse_TX23.h>
#include "xCredentials.h"

#define JSON_BUFFER_LENGTH 100

const char publishTopic[] = "iot-2/evt/status/fmt/json";
const char server[] = ORG ".messaging.internetofthings.ibmcloud.com";
const char authMethod[] = "use-token-auth";
const char token[] = TOKEN;
const char clientId[] = "d:" ORG ":" DEVICE_ID;

WiFiClient wifiClient;
PubSubClient client(server, 1883, wifiClient);
//DATA wire connected to arduino port 10
LaCrosse_TX23 tx23 = LaCrosse_TX23(10);
unsigned long lastRead = 0;
unsigned long lastSent = 0;

void setup() {
  // initialize serial communication with computer:
  Serial.begin(57600);
  while (!Serial) {}

//  wifiConnect();
//  mqttConnect();

}

void loop() {
  unsigned long now = millis();
  bool isOk;
  float ws;
  int wd;

  if (now - lastRead > 5000) {
    // read data from analog input
    lastRead = now;

    isOk = tx23.read(ws, wd);
    Serial.print("WS = "); Serial.print(ws);
    Serial.print(", WD = "); Serial.println(wd);
  }

/*
  if (now - lastSent > 60000) {
    // reconnect and send data
    if (!client.loop()) {
      mqttConnect();
    }

    lastSent = now;
    if(isOk) {
      publishData(ws, wd);
    }
  }
*/
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

void publishData(float ws, int wd) {
  StaticJsonBuffer<JSON_BUFFER_LENGTH> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  JsonObject& d = root.createNestedObject("d");

  d["WS"] = ws;
  d["WD"] = wd;

  char buff[JSON_BUFFER_LENGTH];
  root.printTo(buff, JSON_BUFFER_LENGTH);

  client.publish(publishTopic, buff);
}


