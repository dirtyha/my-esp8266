#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "xCredentials.h"

#define DEVICE_TYPE "EnergyMeter"
#define OUTPUT_BUFFER_LENGTH 300
#define DEBUG 1

const char publishTopic[] = "events/" DEVICE_TYPE "/" DEVICE_ID;               // publish events here
const char server[] = "myhomeat.cloud";
const char authMethod[] = "use-token-auth";
const char token[] = TOKEN;
const char clientId[] = "d:" DEVICE_TYPE ":" DEVICE_ID;

WiFiClient wifiClient;
PubSubClient client(server, 1883, wifiClient);
char output_buffer[OUTPUT_BUFFER_LENGTH];

const int interruptPin = 4;
volatile unsigned long counter = 0;
volatile unsigned long lastImpulse = 0;
volatile long currentWatt = 0;
volatile long sum = 0;
volatile unsigned long lastReported = 0;

void setup() {  
  Serial.begin(115200);
  Serial.setDebugOutput(true);
                
  wifiConnect();
  mqttConnect();

  attachInterrupt(digitalPinToInterrupt(interruptPin), blink, FALLING);

  if (DEBUG) {
    Serial.println("Setup done");
  }
}

void loop() {
  // check that we are connected
  if (!client.loop()) {
    mqttConnect();
  }

  unsigned long now = millis();
  if (now - lastReported > 10000) {
    lastReported = now;
    Serial.print("Watts:");Serial.println(currentWatt);
    publishData();
    reset();
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

boolean publishData() {
  StaticJsonBuffer<OUTPUT_BUFFER_LENGTH> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  JsonObject& d = root.createNestedObject("d");

  d["power"] = currentWatt;

  return publishPayload(root);
}

void reset() {
  sum = 0;
  counter = 0;
  currentWatt = 0;
}

ICACHE_RAM_ATTR void blink() {
  unsigned long diffImpulse;
  unsigned long currentImpulse = millis();

  if (lastImpulse == 0) {
    lastImpulse = currentImpulse;
    return;
  }
  
  // Calculate current Watt usage by measuring the time difference between two impulses
  if (currentImpulse - lastImpulse > 100) { // do not count impulses < 100ms apart
    Serial.print("BLINK:");Serial.println(currentImpulse);
    
    if (currentImpulse > lastImpulse) {
      diffImpulse = currentImpulse - lastImpulse;
      sum += long((3600UL * 1000UL) / diffImpulse);
      counter++;
    } else if (lastImpulse > currentImpulse) { // overflow
      diffImpulse = currentImpulse + (4294967295UL - lastImpulse);
      sum += long((3600UL * 1000UL) / diffImpulse);
      counter++;
    }

    if (counter > 0) {
      currentWatt = sum / counter;
    }
    
    lastImpulse = currentImpulse;
  }
}
