#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "xCredentials.h"

#define DEVICE_TYPE "Ventillator"
#define JSON_BUFFER_LENGTH 150

// watson iot stuff
const char publishTopic[] = "events/" DEVICE_TYPE "/" DEVICE_ID;          // publish measurements here
const char updateTopic[] = "cmd/" DEVICE_TYPE "/" DEVICE_ID;           // subscribe for update command
const char server[] = "myhomeat.cloud";
const char authMethod[] = "use-token-auth";
const char token[] = TOKEN;
const char clientId[] = "d:" DEVICE_TYPE ":" DEVICE_ID;

WiFiClient wifiClient;
void myCallback(char* topic, byte* payload, unsigned int payloadLength);
PubSubClient client(server, 1883, myCallback, wifiClient);
boolean isOn = false;

void setup() {
  pinMode(D2, OUTPUT);
  pinMode(D3, OUTPUT);
  ventillationOff();

  // initialize serial communication with computer:
  Serial.begin(57600);
  delay(2000);

  Serial.println("Setup started");

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

void wifiConnect() {
  Serial.print("Connecting to "); Serial.print(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level)
    delay(500);
    digitalWrite(LED_BUILTIN, LOW);   // turn the LED on (HIGH is the voltage level)
    Serial.print(".");
  }
  WiFi.mode(WIFI_STA);
  Serial.print("WiFi connected, IP address: "); Serial.println(WiFi.localIP());
  digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level)
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
      Serial.println("Subscribe to update OK");
    } else {
      Serial.println("Subscribe to update FAILED");
    }
  }
}

boolean publish() {
  boolean ret = true;

  StaticJsonBuffer<JSON_BUFFER_LENGTH> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  JsonObject& d = root.createNestedObject("d");

  if(isOn) {
    d["STATE"] = "ON";
  } else {
    d["STATE"] = "OFF";
  }

  Serial.println("Publish payload:"); root.prettyPrintTo(Serial); Serial.println();

  char buff[JSON_BUFFER_LENGTH];
  root.printTo(buff, JSON_BUFFER_LENGTH);

  if (client.publish(publishTopic, buff)) {
    Serial.println("Publish OK");
  } else {
    Serial.println("Publish FAILED");
    ret = false;
  }

  return ret;
}

void myCallback(char* topic, byte * payload, unsigned int length) {
  Serial.print("Callback invoked for topic: "); Serial.println(topic);

  if (strcmp (updateTopic, topic) == 0) {
    handleUpdate(payload);
    publish();
  }

}

void handleUpdate(byte * payload) {
  StaticJsonBuffer<JSON_BUFFER_LENGTH> jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject((char*)payload);
  if (!root.success()) {
    Serial.println("handleUpdate: payload parse FAILED");
    return;
  }

  Serial.println("handleUpdate payload:"); root.prettyPrintTo(Serial); Serial.println();

  JsonObject& d = root["d"];
  if (d.containsKey("CMD")) {
    if (d["CMD"] == "ON") {
      ventillationOn();
    } else {
      ventillationOff();
    }
  }
}

void ventillationOn() {
  digitalWrite(D3, HIGH);
  delay(500);           
  digitalWrite(D3, LOW); 
  isOn = true;
  Serial.println("Ventillation ON");
}

void ventillationOff() {
  digitalWrite(D2, HIGH);
  delay(500);           
  digitalWrite(D2, LOW); 
  isOn = false;
  Serial.println("Ventillation OFF");
}
