#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <MitsubishiHeatpumpIR.h>
#include "xCredentials.h"

#define DEVICE_TYPE "Heatpump"
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

IRSenderBitBang irSender(4);  // IR led on Wemos D1 mini, connect between D2 and G

MitsubishiFDHeatpumpIR *heatpumpIR = new MitsubishiFDHeatpumpIR();

void setup() {
  Serial.begin(57600);
  delay(1000);

  wifiConnect();
  mqttConnect();

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

boolean publishPayload(JsonObject& root) {
  boolean ret = true;

  if (DEBUG) {
    Serial.println("Publish payload:"); root.prettyPrintTo(Serial); Serial.println();
  }

  char buff[JSON_BUFFER_LENGTH];
  root.printTo(buff, JSON_BUFFER_LENGTH);

  if (client.publish(publishTopic, buff)) {
    if (DEBUG) {
      Serial.println("Publish OK");
    }
  } else {
    if (DEBUG) {
      Serial.println("Publish FAILED");
    }
    ret = false;
  }

  return ret;
}

boolean publishData(int tyhja) {
  StaticJsonBuffer<JSON_BUFFER_LENGTH> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  JsonObject& d = root.createNestedObject("d");

//  d["module"] = io.getModule();

  return publishPayload(root);
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

  uint8_t power = d["power"] == "on" ? POWER_ON : POWER_OFF;
  uint8_t mode = d["mode"] == "heat" ? MODE_HEAT : MODE_COOL;
  uint8_t fan = d["fan"];
  uint8_t temp = d["temp"];

  // Send the IR command
  heatpumpIR->send(irSender, power, mode, fan, temp, VDIR_MDOWN, HDIR_MIDDLE);
}
