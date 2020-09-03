#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ArduinoJson.h>
#include <RCSwitchCustom.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "xCredentials.h"

#define JSON_BUFFER_LENGTH 150
#define ONE_WIRE_BUS 14
#define TX_PIN 4
#define HALL_PIN 5
#define DEBUG 1

#define TX_CMD_OPEN  "FQ0F00Q101011F0F0F0F"
#define TX_CMD_CLOSE "FQ0F00Q101011F0F0101"
#define TX_CMD_STOP  "FQ0F00Q101011F0FFFFF"

const char publishTopic[] = "events/" DEVICE_ID; // publish events here
const char cmdTopic[] = "cmd/" DEVICE_ID;        // subscribe for commands here
const char AWS_endpoint[] = AWS_PREFIX ".iot.eu-west-1.amazonaws.com";
void callback(char* topic, byte* payload, unsigned int payloadLength);
const char clientId[] = "ESP8266-" DEVICE_ID;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");
WiFiClientSecure espClient;
PubSubClient client(AWS_endpoint, 8883, callback, espClient);
CRCSwitch mySwitch = CRCSwitch();
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensor(&oneWire);
unsigned long lastHeartBeat = 0;
boolean lastIsClosed = true;

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);

  setup_wifi();
  delay(1000);
  if (!SPIFFS.begin()) {
    Serial.println("Failed to mount file system");
    return;
  }

  Serial.print("Heap: "); Serial.println(ESP.getFreeHeap());

  // Load certificate file
  File cert = SPIFFS.open("/cert.der", "r");
  if (!cert) {
    Serial.println("Failed to open cert file");
  }
  else
    Serial.println("Success to open cert file");

  delay(1000);

  if (espClient.loadCertificate(cert))
    Serial.println("cert loaded");
  else
    Serial.println("cert not loaded");

  // Load private key file
  File private_key = SPIFFS.open("/private.der", "r");
  if (!private_key) {
    Serial.println("Failed to open private cert file");
  }
  else
    Serial.println("Success to open private cert file");

  delay(1000);

  if (espClient.loadPrivateKey(private_key))
    Serial.println("private key loaded");
  else
    Serial.println("private key not loaded");

  // Load CA file
  File ca = SPIFFS.open("/ca.der", "r");
  if (!ca) {
    Serial.println("Failed to open ca ");
  }
  else
    Serial.println("Success to open ca");

  delay(1000);

  if (espClient.loadCACert(ca))
    Serial.println("ca loaded");
  else
    Serial.println("ca failed");

  Serial.print("Heap: "); Serial.println(ESP.getFreeHeap());

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

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

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

void setup_wifi() {

  delay(10);
  // We start by connecting to a WiFi network
  espClient.setBufferSizes(512, 512);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  WiFi.mode(WIFI_STA);
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  timeClient.begin();
  while (!timeClient.update()) {
    timeClient.forceUpdate();
  }

  espClient.setX509Time(timeClient.getEpochTime());

}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(clientId)) {
      Serial.println("connected");
      // resubscribe
      client.subscribe(cmdTopic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");

      char buf[256];
      espClient.getLastSSLError(buf, 256);
      Serial.print("WiFiClientSecure SSL error: ");
      Serial.println(buf);

      // Wait 5 seconds before retrying
      delay(5000);
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

void callback(char* topic, byte * payload, unsigned int length) {
  Serial.print("Callback invoked for topic: "); Serial.println(topic);

  if (strcmp (cmdTopic, topic) == 0) {
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
