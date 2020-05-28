#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "xCredentials.h"

#define DEVICE_TYPE "Relay"
#define JSON_BUFFER_LENGTH 500
#define DEBUG 1

const char publishTopic[] = "events/" DEVICE_TYPE "/" DEVICE_ID;               // publish events here
const char cmdTopic[] = "cmd/" DEVICE_TYPE "/" DEVICE_ID;             // subscribe for commands here
const char server[] = "myhomeat.cloud";
const char authMethod[] = "use-token-auth";
const char token[] = TOKEN;
const char clientId[] = "d:" DEVICE_TYPE ":" DEVICE_ID;

WiFiClient wifiClient;
void myCallback(char* topic, byte* payload, unsigned int payloadLength);
PubSubClient client(server, 1883, myCallback, wifiClient);

void setup() {
  Serial.begin(57600);
  delay(1000);

  wifiConnect();
  mqttConnect();

  pinMode(5, OUTPUT);

  if (DEBUG) {
    Serial.println("Setup done");
  }
}

void loop() {

  // check that we are connected
  if (!client.loop()) {
    mqttConnect();
  }
}

void wifiConnect() {
  if (DEBUG) {
    Serial.print("Connecting to "); Serial.print(ssid);
  }
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    if (DEBUG) {
      Serial.print(".");
    }
  }
  WiFi.mode(WIFI_STA);
  if (DEBUG) {
    Serial.print("WiFi connected, IP address: "); Serial.println(WiFi.localIP());
  }
}

void mqttConnect() {
  if (!!!client.connected()) {
    if (DEBUG) {
      Serial.print("Reconnecting MQTT client to "); Serial.println(server);
    }
    int count = 20;
    while (count-- > 0 && !!!client.connect(clientId, authMethod, token)) {
      if (DEBUG) {
        Serial.print(".");
      }
      delay(500);
    }
    if (DEBUG) {
      Serial.println();
    }

    if (client.subscribe(cmdTopic, 1)) {
      if (DEBUG) {
        Serial.println("Subscribe to update OK");
      }
    } else {
      if (DEBUG) {
        Serial.println("Subscribe to update FAILED");
      }
    }
  }
}

void myCallback(char* topic, byte * payload, unsigned int length) {
  if (DEBUG) {
    Serial.print("Callback invoked for topic: "); Serial.println(topic);
  }

  if (strcmp (cmdTopic, topic) == 0) {
    handleUpdate(payload);
  }
}

void handleUpdate(byte * payload) {
  StaticJsonBuffer<JSON_BUFFER_LENGTH> jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject((char*)payload);
  if (!root.success()) {
    if (DEBUG) {
      Serial.println("handleUpdate: payload parse FAILED");
    }
    return;
  }
  if (DEBUG) {
    Serial.println("handleUpdate payload:"); root.prettyPrintTo(Serial); Serial.println();
  }

  JsonObject& d = root["d"];

  if (d.containsKey("cmd")) {
    String cmd = d["cmd"];

    if (cmd == "pulse") {
      pulse();
    }
  }
}

void pulse() {
  digitalWrite(5, HIGH);
  delay(500);           
  digitalWrite(5, LOW); 
}
