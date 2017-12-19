// Vallox Digit SE monitoring and control for ESP8266
// requires RS485 serial line adapter between ESP8266 <-> DigitSE
// and account in Watson IoT platform

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Vallox.h>
#include "xCredentials.h"

#define DEVICE_TYPE "VentillationMachine"
#define JSON_BUFFER_LENGTH 300
#define DEBUG true

const char publishTopic[] = "iot-2/evt/status/fmt/json";          // publish measurements here
const char updateTopic[] = "iot-2/cmd/update/fmt/json";           // subscribe for update command

// watson iot stuff
char server[] = ORG ".messaging.internetofthings.ibmcloud.com";
char authMethod[] = "use-token-auth";
char token[] = TOKEN;
char clientId[] = "d:" ORG ":" DEVICE_TYPE ":" DEVICE_ID;

WiFiClient wifiClient;
void myCallback(char* topic, byte* payload, unsigned int payloadLength);
PubSubClient client(server, 1883, myCallback, wifiClient);
Vallox vx(D1, D2, DEBUG);
unsigned long lastUpdated;

void setup() {
  Serial.begin(9600);

  wifiConnect();
  vx.init();
  lastUpdated = vx.getUpdated();
  mqttConnect();

  Serial.println("Setup done.");
}

void loop() {

  // loop VX messages
  vx.loop();

  // check that we are connected
  if (!client.loop()) {
    mqttConnect();
  }

  unsigned long newUpdate = vx.getUpdated();
  if (lastUpdated != newUpdate) {
    // data hash changed
    if (publishData()) {
      lastUpdated = newUpdate;
    }
    if (DEBUG) {
      prettyPrint();
    }
  }
}

void prettyPrint() {
  Serial.print("Inside air T="); Serial.print(vx.getInsideTemp()); Serial.print(", ");
  Serial.print("Outside air T="); Serial.print(vx.getOutsideTemp()); Serial.print(", ");
  Serial.print("Inbound air T="); Serial.print(vx.getIncomingTemp()); Serial.print(", ");
  Serial.print("Outbound air T="); Serial.print(vx.getExhaustTemp()); Serial.print(", ");
  Serial.print("Fan speed="); Serial.print(vx.getFanSpeed());
  Serial.println();
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

boolean publishData() {
  boolean ret = true;

  StaticJsonBuffer<JSON_BUFFER_LENGTH> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  JsonObject& d = root.createNestedObject("d");

  if (vxIsSet(vx.getInsideTemp())) {
    d["T_IN"] = vx.getInsideTemp();
  }

  if (vxIsSet(vx.getOutsideTemp())) {
    d["T_OUT"] = vx.getOutsideTemp();
  }

  if (vxIsSet(vx.getInsideTemp())) {
    d["T_INB"] = vx.getIncomingTemp();
  }

  if (vxIsSet(vx.getExhaustTemp())) {
    d["T_OUTB"] = vx.getExhaustTemp();
  }

  if (vxIsSet(vx.getRH())) {
    d["RH"] = vx.getRH();
  }

  d["ON"] = vx.isOn();
  d["RH_MODE"] = vx.isRHMode();
  d["HEATING_MODE"] = vx.isHeatingMode();
  d["SUMMER_MODE"] = vx.isSummerMode();
  d["HEATING"] = vx.isHeating();
  d["FAULT"] = vx.isFault();
  d["SERVICE"] = vx.isService();
  d["SPEED"] = vx.getFanSpeed();
  d["DEFAULT_SPEED"] = vx.getDefaultFanSpeed();
  d["SERVICE_PERIOD"] = vx.getServicePeriod();
  d["SERVICE_COUNTER"] = vx.getServiceCounter();
  d["HEATING_TARGET"] = vx.getHeatingTarget();

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
    Serial.println("Publish FAILED");
    ret = false;
  }

  return ret;
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
  if (d.containsKey("speed")) {
    int speed = d["speed"];
    vx.setFanSpeed(speed);
  }
}


