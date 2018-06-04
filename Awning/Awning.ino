#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ArduinoJson.h>
#include <RCSwitch.h>
#include "xCredentials.h"

#define DEVICE_TYPE "Motor"
#define JSON_BUFFER_LENGTH 150
#define ONE_WIRE_BUS D5
#define TX_PIN D2
#define HALL_PIN D1

#define TX_CMD_OPEN  "FQ0F00Q101011F0F0F0F"
#define TX_CMD_CLOSE "FQ0F00Q101011F0F0101"
#define TX_CMD_STOP  "FQ0F00Q101011F0FFFFF"

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
RCSwitch mySwitch = RCSwitch();
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensor(&oneWire);
unsigned long lastHeartBeat = 0;
boolean lastIsClosed = true;

void setup() {
  // initialize serial communication with computer:
  Serial.begin(57600);
  delay(2000);

  Serial.println("Setup started");

  wifiConnect();
  mqttConnect();

  // TX setup
  mySwitch.enableTransmit(TX_PIN);
  mySwitch.setProtocol(4);
  mySwitch.setRepeatTransmit(10);

  pinMode(HALL_PIN, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);

  digitalWrite(LED_BUILTIN, LOW);    // turn the LED off by making the voltage LOW

  Serial.println("Setup done");
}

void loop() {
  unsigned long now = millis();
  boolean isClosed = true;

  if (digitalRead(HALL_PIN) == HIGH) {
    isClosed = false;
  }

  // check that we are connected
  if (!client.loop()) {
    mqttConnect();
  }

  if (lastIsClosed != isClosed) {
    float ta = poll();
    if(publish(isClosed, ta)) {
      lastIsClosed = isClosed;    
    }
  }

  if ( now - lastHeartBeat > 600000) {
    float ta = poll();
    if (publish(isClosed, ta)) {
      lastHeartBeat = now;
    }
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

float poll() {
  sensor.begin();
  sensor.requestTemperatures();

  return sensor.getTempCByIndex(0);
}

boolean publish(boolean isClosed, float ta) {
  boolean ret = true;

  StaticJsonBuffer<JSON_BUFFER_LENGTH> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  JsonObject& d = root.createNestedObject("d");

  d["TA"] = ta;
  if(isClosed) {
    d["STATE"] = "CLOSED";
  } else {
    d["STATE"] = "OPEN";
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

void openIt() {
  Serial.println("Transmitting OPEN.");
  mySwitch.sendQuadState(TX_CMD_OPEN);
  mySwitch.sendQuadState(TX_CMD_OPEN);
}

void stopIt() {
  Serial.println("Transmitting STOP.");
  mySwitch.sendQuadState(TX_CMD_STOP);
  mySwitch.sendQuadState(TX_CMD_STOP);
}

void closeIt() {
  Serial.println("Transmitting CLOSE.");
  mySwitch.sendQuadState(TX_CMD_CLOSE);
  mySwitch.sendQuadState(TX_CMD_CLOSE);
}

void myCallback(char* topic, byte * payload, unsigned int length) {
  Serial.print("Callback invoked for topic: "); Serial.println(topic);

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

  Serial.println("handleUpdate payload:"); root.prettyPrintTo(Serial); Serial.println();

  JsonObject& d = root["d"];
  if (d.containsKey("CMD")) {
    if (d["CMD"] == "OPEN") {
      openIt();
    } else if (d["CMD"] == "CLOSE") {
      closeIt();
    } else if (d["CMD"] == "STOP") {
      stopIt();
    }
  }
}
