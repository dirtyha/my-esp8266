#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <SoftwareSerial.h>
#include <ArduinoJson.h>
#include <IHC.h>
#include "IHCConfig.h"
#include "xCredentials.h"

#define DEVICE_TYPE "IHCConnector"
#define JSON_BUFFER_LENGTH 500
#define DEBUG 1
#define DEBUG_IHC 0

const char publishTopic[] = "events/" DEVICE_TYPE "/" DEVICE_ID;               // publish events here
const char cmdTopic[] = "cmd/" DEVICE_TYPE "/" DEVICE_ID;             // subscribe for commands here
const char server[] = "myhomeat.cloud";
const char authMethod[] = "use-token-auth";
const char token[] = TOKEN;
const char clientId[] = "d:" DEVICE_TYPE ":" DEVICE_ID;

WiFiClient wifiClient;
void myCallback(char* topic, byte* payload, unsigned int payloadLength);
PubSubClient client(server, 1883, myCallback, wifiClient);
SoftwareSerial sws(4, 5);
IHC ihc;
IHCRS485Packet sendPacket;
int oldStatus = 0;

void setup() {
  Serial.begin(57600);
  delay(1000);

  wifiConnect();
  mqttConnect();

  ihc.init(&sws, DEBUG_IHC);

  if (DEBUG) {
    Serial.println("Setup done");
  }
}

void loop() {

  // check that we are connected
  if (!client.loop()) {
    mqttConnect();
  }

  ihc.loop();
  int status = ihc.getStatus();
  if (oldStatus != status) {
    oldStatus = status;
    publishStatus(status);
  }

  IHCRS485Packet *packet = ihc.receive();
  if (packet != NULL) {
    switch (packet->getDataType()) {
      case IHCDefs::OUTP_STATE:
      {
        unsigned long changeId = updateStates(packet->getData());
        if (changeId > 0) {
          for (int i = 0; i < SIZE; i++) {
            IHCIO io = ios[i];
            if (io.getChangeId() == changeId) {
              publishData(io);
            }
          }
        }
      }
      break;

      default:
        publishMystery(packet->getDataType());
    }
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

boolean publishData(IHCIO & io) {
  StaticJsonBuffer<JSON_BUFFER_LENGTH> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  JsonObject& d = root.createNestedObject("d");

  d["module"] = io.getModule();
  d["port"] = io.getPort();
  d["name"] = io.getName();
  d["type"] = io.isOutput() ? "output" : "input";
  d["state"] = io.getState();

  return publishPayload(root);
}

boolean publishStatus(int status) {
  boolean ret = true;

  StaticJsonBuffer<JSON_BUFFER_LENGTH> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  JsonObject& d = root.createNestedObject("d");

  d["status"] = status;

  return publishPayload(root);
}

boolean publishMystery(byte dataType) {
  boolean ret = true;

  StaticJsonBuffer<JSON_BUFFER_LENGTH> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  JsonObject& d = root.createNestedObject("d");

  d["dataType"] = dataType;

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

  if (d.containsKey("cmd")) {
    String cmd = d["cmd"];

    if (cmd == "status") {
      // Publish status
      publishStatus(oldStatus);
    } else if (cmd == "data") {
      // Publish data
      for (int i = 0; i < SIZE; i++)
      {
        publishData(ios[i]);
      }
    } else if (cmd == "change") {
      // Change output
      if (d.containsKey("module")) {
        unsigned short module = d["module"];
        if (d.containsKey("port")) {
          unsigned short port = d["port"];
          if (d.containsKey("type")) {
            String type = d["type"];
            if (type.equals("output")) {
              if (d.containsKey("state"))
              {
                bool state = d["state"];
                Vector<byte> *pData = changeOutput(module, port, state);
                sendPacket.setData(IHCDefs::ID_IHC, IHCDefs::SET_OUTPUT, pData);
                ihc.send(&sendPacket);
              }
            }
          }
        }
      }
    }
  }
}
