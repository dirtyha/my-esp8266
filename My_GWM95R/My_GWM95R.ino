#include <ESP8266WiFi.h>
#include <ModbusMaster.h>
#include <PubSubClient.h>
#include "xCredentials.h"

#define DEVICE_TYPE "MY_GWM95R"

char server[] = ORG ".messaging.internetofthings.ibmcloud.com";
char authMethod[] = "use-token-auth";
char token[] = TOKEN;
char clientId[] = "d:" ORG ":" DEVICE_TYPE ":" DEVICE_ID;

const char publishTopic[] = "iot-2/evt/status/fmt/json";

WiFiClient wifiClient;
PubSubClient client(server, 1883, wifiClient);
ModbusMaster node;

long lastPublishMillis;

typedef struct {
  float ta;
  float td;
  float rh;
  float a;
  int co2;
} wt_result;

/*
void setup() {
  Serial.begin(9600, SERIAL_8N2);
  while(!Serial) {}

  wifiConnect();
  mqttConnect();

  // configure Serial port 0 and modbus slave 1
  node.begin(1, Serial);
}
*/

void setup() {
  Serial.begin(9600, SERIAL_8N2);
  while(!Serial) {}

  wifiConnect();
  mqttConnect();

  // configure Serial port 0 and modbus slave 1
  node.begin(1, Serial);
  publishData();
  
  mqttDisconnect();
  wifiDisconnect();  

  // sleep 10 minutes
  ESP.deepSleep(600e6);
}

/*
void loop() {
  if (millis() - lastPublishMillis > 60000) {
    publishData();
    lastPublishMillis = millis();
  }

  if (!client.loop()) {
    mqttConnect();
  }
}
*/

void loop() {
}


wt_result* poll() {
  wt_result* ret = NULL;

  // slave: read 3 16-bit registers starting at register 2 to RX buffer
  uint8_t result = node.readHoldingRegisters(256, 10);
  
  // do something with data if read is successful
  if (result == node.ku8MBSuccess)
  {
    ret = (wt_result*)malloc(sizeof(wt_result));
    if (ret != NULL) {
      ret->co2 = node.getResponseBuffer(0);
      ret->rh = node.getResponseBuffer(1) * 0.01f;
      ret->ta = node.getResponseBuffer(2) * 0.01f;
      ret->td = node.getResponseBuffer(3) * 0.01f;
      ret->a = node.getResponseBuffer(7) * 0.01f;
    } else {
      Serial.println("Malloc failed.");
    }
  } else {
    Serial.println("Failed to read modbus.");
  }

  return ret;
}

void wifiConnect() {
//  Serial.print("Connecting to "); Serial.print(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
//    Serial.print(".");
  }
//  Serial.print("WiFi connected, IP address: "); Serial.println(WiFi.localIP());
}

void wifiDisconnect() {
  if(WiFi.status() == WL_CONNECTED) {
    WiFi.disconnect();
  }
}

void mqttConnect() {
  if (!!!client.connected()) {
//    Serial.print("Reconnecting MQTT client to "); Serial.println(server);
    while (!!!client.connect(clientId, authMethod, token)) {
//      Serial.print(".");
      delay(500);
    }
//    Serial.println();
  }
}

void mqttDisconnect() {
  if(client.connected()) {
    client.disconnect();
  }
}

void publishData() {
  wt_result* result = poll();
  if (result != NULL) {
    // construct a JSON response
    String json = "{\"d\":{";
    json += "\"TA\":";
    json += result->ta;
    json += ",\"Td\":";
    json += result->td;
    json += ",\"RH\":";
    json += result->rh;
    json += ",\"a\":";
    json += result->a;
    json += ",\"CO2\":";
    json += result->co2;
    json += "}}";

    if (client.publish(publishTopic, (char*) json.c_str())) {
      Serial.println("Publish OK");
    } else {
      Serial.println("Publish FAILED");
    }
  }
}

