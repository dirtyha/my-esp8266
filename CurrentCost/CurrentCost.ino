#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <SoftwareSerial.h>
#include <ArduinoJson.h>
#include "xCredentials.h"

#define DEVICE_TYPE "EnergyMeter"
#define INPUT_BUFFER_LENGTH 500
#define OUTPUT_BUFFER_LENGTH 500
#define DEBUG 1
#define NO_DATA '!'

const char publishTopic[] = "events/" DEVICE_TYPE "/" DEVICE_ID;               // publish events here
const char server[] = "myhomeat.cloud";
const char authMethod[] = "use-token-auth";
const char token[] = TOKEN;
const char clientId[] = "d:" DEVICE_TYPE ":" DEVICE_ID;

WiFiClient wifiClient;
PubSubClient client(server, 1883, wifiClient);
SoftwareSerial sws(5, 0, true);
char input_buffer[INPUT_BUFFER_LENGTH];
char output_buffer[OUTPUT_BUFFER_LENGTH];

void setup() {
  Serial.begin(57600);
  delay(1000);

  wifiConnect();
  mqttConnect();

  sws.begin(57600);

  if (DEBUG) {
    Serial.println("Setup done");
  }
}

void loop() {

  // check that we are connected
  if (!client.loop()) {
    mqttConnect();
  }

  char* pMsg = readMsg();
  if (pMsg != NULL) {
    publish(pMsg);
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
  }
}

char *readMsg() {
  unsigned short i = 0;

  input_buffer[i] = readChar();
  while(input_buffer[i] != NO_DATA && input_buffer[i] != '\r' && i < INPUT_BUFFER_LENGTH - 1) {
    input_buffer[++i] = readChar();
  }
  input_buffer[++i] = '\0';

  if (isComplete(input_buffer)) {
    if (DEBUG) {
      Serial.println(input_buffer);
    }

    return input_buffer;
  }

  return NULL;
}

bool isComplete(char *pMsg) {
  bool ret = false;

  unsigned short len = strlen(pMsg);
  if (strlen(pMsg) > 10) {
//    if (pMsg[0] == '<' && pMsg[1] == 'm' && pMsg[2] == 's' && pMsg[3] == 'g' && pMsg[4] == '>') {
//      if (pMsg[len - 7] == '<' && pMsg[len - 6] == '/' && pMsg[len - 5] == 'm' && pMsg[len - 4] == 's' && pMsg[len - 3] == 'g' && pMsg[len - 2] == '>') {
        ret = true;
//      }
//    }
  }
  
  return ret;
}

char readChar() {
  unsigned long start = millis();
  while (1) {
    if (millis() - start > 1000) {
      break;
    }
    
    if (sws.available() > 0) {
      char c = (char)sws.read();
      return c;
    }
  }

  return NO_DATA;  
}

boolean publishPayload(JsonObject& root) {
  boolean ret = true;

  if (DEBUG) {
    Serial.println("Publish payload:"); root.prettyPrintTo(Serial); Serial.println();
  }

  root.printTo(output_buffer, OUTPUT_BUFFER_LENGTH);

  if (client.publish(publishTopic, output_buffer)) {
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

boolean publish(char *pMsg) {
  StaticJsonBuffer<OUTPUT_BUFFER_LENGTH> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  JsonObject& d = root.createNestedObject("d");

  d["msg"] = pMsg;

  return publishPayload(root);
}
