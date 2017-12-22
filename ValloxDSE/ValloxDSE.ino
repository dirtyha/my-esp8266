// Vallox Digit SE monitoring and control for ESP8266
// requires RS485 serial line adapter between ESP8266 <-> DigitSE
// and account in Watson IoT platform
// NOTE: MQTT message size is over 128 bytes. 
// You must increase MQTT_MAX_PACKET_SIZE to 256 in PubSubClient.h

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Vallox.h>
// you must create your own xCredentials.h for Watson IoT and WiFi: 
// ORG, DEVICE_ID, TOKEN, ssid and password
#include "xCredentials.h" 

#define DEVICE_TYPE "VentillationMachine"
#define JSON_BUFFER_LENGTH 350
#define DEBUG true

// watson iot stuff
const char publishTopic[] = "iot-2/evt/status/fmt/json";          // publish measurements here
const char updateTopic[] = "iot-2/cmd/update/fmt/json";           // subscribe for update command
const char server[] = ORG ".messaging.internetofthings.ibmcloud.com";
const char authMethod[] = "use-token-auth";
const char token[] = TOKEN;
const char clientId[] = "d:" ORG ":" DEVICE_TYPE ":" DEVICE_ID;

WiFiClient wifiClient;
void myCallback(char* topic, byte* payload, unsigned int payloadLength);
PubSubClient client(server, 1883, myCallback, wifiClient);
Vallox vx(D1, D2, DEBUG);
unsigned long lastUpdated = 0;

void setup() {
  Serial.begin(9600);

  wifiConnect();
  vx.init();
  mqttConnect();

  Serial.println("Setup done");
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

  d["T_IN"] = vx.getInsideTemp();
  d["T_OUT"] = vx.getOutsideTemp();
  d["T_INB"] = vx.getIncomingTemp();
  d["T_OUTB"] = vx.getExhaustTemp();
  d["ON"] = vx.isOn();
  d["RH_MODE"] = vx.isRhMode();
  d["RH"] = vx.getRh();
  d["HEATING_MODE"] = vx.isHeatingMode();
  d["SUMMER_MODE"] = vx.isSummerMode();
  d["HEATING"] = vx.isHeating();
  d["FAULT"] = vx.isFault();
  d["SERVICE"] = vx.isServiceNeeded();
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
  if (d.containsKey("ON")) {
    boolean isOn = d["ON"];
    if(isOn) {
      vx.setOn();
    } else {
      vx.setOff();
    }
  }
  if (d.containsKey("RH_MODE")) {
    boolean isOn = d["RH_MODE"];
    if(isOn) {
      vx.setRhModeOn();
    } else {
      vx.setRhModeOff();
    }
  }
  if (d.containsKey("HEATING_MODE")) {
    boolean isOn = d["HEATING_MODE"];
    if(isOn) {
      vx.setHeatingModeOn();
    } else {
      vx.setHeatingModeOff();
    }
  }
  if (d.containsKey("SPEED")) {
    int speed = d["SPEED"];
    vx.setFanSpeed(speed);
  }
  if (d.containsKey("HEATING_TARGET")) {
    int ht = d["HEATING_TARGET"];
    vx.setHeatingTarget(ht);
  }
  if (d.containsKey("SERVICE_PERIOD")) {
    int sp = d["SERVICE_PERIOD"];
    vx.setServicePeriod(sp);
  }
  if (d.containsKey("SERVICE_COUNTER")) {
    int sc = d["SERVICE_COUNTER"];
    vx.setServiceCounter(sc);
  }
}


