#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <ArduinoJson.h>
#include "xCredentials.h"

#define DEVICE_TYPE "MP3_Player"
#define JSON_BUFFER_LENGTH 350
#define DEBUG true
#define DEVICE_ADDRESS 8

// watson iot stuff
const char updateTopic[] = "cmd/" DEVICE_TYPE "/" DEVICE_ID;           // subscribe for update command
const char server[] = "myhomeat.cloud";
const char authMethod[] = "use-token-auth";
const char token[] = TOKEN;
const char clientId[] = "d:" DEVICE_TYPE ":" DEVICE_ID;

WiFiClient wifiClient;
void myCallback(char* topic, byte* payload, unsigned int payloadLength);
PubSubClient client(server, 1883, myCallback, wifiClient);

void setup() {
  Serial.begin(9600);
  Wire.begin(D1, D2);

  wifiConnect();
  mqttConnect();

  Serial.println("Setup done");
}

void loop() {
  // check that we are connected
  if (!client.loop()) {
    mqttConnect();
  }
}

void send(byte volume, const int files[], byte size) {
  Wire.beginTransmission(DEVICE_ADDRESS);

  Wire.write(0xFF);
  Wire.write(volume);
  Wire.write(size);
  for (int i = 0; i < size; i++) {
    int id = files[i];
    Serial.print("Sending #"); Serial.println(id);
    const byte high = ((id >> 7) & 0x7f) | 0x80;
    const byte low  = (id & 0x7f);

    Wire.write(low);
    Wire.write(high);
  }

  Wire.endTransmission();
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
    int count = 20;
    while (count-- > 0 && !!!client.connect(clientId, authMethod, token)) {
      Serial.print(".");
      delay(500);
    }
    Serial.println();

    if (client.subscribe(updateTopic, 1)) {
      if (DEBUG) {
        Serial.println("Subscribe to update OK");
      }
    } else {
      Serial.println("Subscribe to update FAILED");
    }
  }
}

void myCallback(char* topic, byte * payload, unsigned int length) {
  if (DEBUG) {
    Serial.print("Callback invoked for topic: "); Serial.println(topic);
  }

  if (strcmp (updateTopic, topic) == 0) {
    handleUpdate(payload);
  }
}

void handleUpdate(byte * payload) {
  StaticJsonBuffer<JSON_BUFFER_LENGTH> jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject((char*)payload);
  if (!root.success()) {
    Serial.println("handleUpdate: payload parse FAILED");
    return;
  }

  if (DEBUG) {
    Serial.println("handleUpdate payload:"); root.prettyPrintTo(Serial); Serial.println();
  }

  JsonObject& d = root["d"];

  byte volume = 30;
  if (d.containsKey("VOLUME")) {
    volume = d["VOLUME"];
  }

  if (d.containsKey("FILES")) {
    int files[10];

    JsonArray& filesArray = d["FILES"];
    byte size = filesArray.size();
    int j = 0;
    for (int i = size - 1; i >= 0; i--) {
      files[j++] = filesArray.get<int>(i);
    }
    send(volume, files, size);
  }
}



