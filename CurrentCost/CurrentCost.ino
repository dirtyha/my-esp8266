#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "xCredentials.h"

#define DEVICE_TYPE "EnergyMeter"
#define INPUT_BUFFER_LENGTH 230
#define OUTPUT_BUFFER_LENGTH 300
#define NO_DATA '!'

const char publishTopic[] = "events/" DEVICE_TYPE "/" DEVICE_ID;               // publish events here
const char server[] = "myhomeat.cloud";
const char authMethod[] = "use-token-auth";
const char token[] = TOKEN;
const char clientId[] = "d:" DEVICE_TYPE ":" DEVICE_ID;

WiFiClient wifiClient;
PubSubClient client(server, 1883, wifiClient);
char input_buffer[INPUT_BUFFER_LENGTH];
char output_buffer[OUTPUT_BUFFER_LENGTH];

void setup() {
  Serial.begin(57600);
  U0C0 = BIT(UCRXI) | BIT(UCBN) | BIT(UCBN+1) | BIT(UCSBN); // Inverse RX

  wifiConnect();
  mqttConnect();

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
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
  WiFi.mode(WIFI_STA);
}

void mqttConnect() {
  if (!!!client.connected()) {
    int count = 20;
    while (count-- > 0 && !!!client.connect(clientId, authMethod, token)) {
      delay(500);
    }
  }
}

char *readMsg() {
  unsigned short i = 0;

  input_buffer[i] = readChar();
  while(input_buffer[i] != NO_DATA && input_buffer[i] != '\r' && input_buffer[i] != '\n' && i < INPUT_BUFFER_LENGTH - 1) {
    input_buffer[++i] = readChar();
  }
  input_buffer[i] = '\0';

  if (isComplete(input_buffer)) {
    return input_buffer;
  }

  return NULL;
}

bool isComplete(char *pMsg) {
  bool ret = false;

  unsigned short len = strlen(pMsg);
  if (strlen(pMsg) > 220) {
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
    if (millis() - start > 5000) {
      break;
    }

    if (Serial.available() > 0) {
      char c = Serial.read();
      return c;
    }
  }

  return NO_DATA;  
}

boolean publishPayload(JsonObject& root) {
  boolean ret = true;

  root.printTo(output_buffer, OUTPUT_BUFFER_LENGTH);

  if (client.publish(publishTopic, output_buffer)) {
  } else {
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
