#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <MitsubishiHeatpumpIR.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "xCredentials.h"

#define JSON_BUFFER_LENGTH 300
#define DEBUG 1

const char publishTopic[] = "events/" DEVICE_ID; // publish events here
const char cmdTopic[] = "cmd/" DEVICE_ID;        // subscribe for commands here
const char AWS_endpoint[] = AWS_PREFIX ".iot.eu-west-1.amazonaws.com";
void callback(char* topic, byte* payload, unsigned int payloadLength);
const char clientId[] = "ESP8266-" DEVICE_ID;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");
WiFiClientSecure espClient;
PubSubClient client(AWS_endpoint, 8883, callback, espClient);

IRSenderBitBang irSender(4);  // IR led on Wemos D1 mini, connect between D2 and G

MitsubishiFDHeatpumpIR *heatpumpIR = new MitsubishiFDHeatpumpIR();

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

  if (DEBUG) {
    Serial.println("Setup done");
  }
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
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

boolean publishResponse() {
  StaticJsonBuffer<JSON_BUFFER_LENGTH> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  JsonObject& d = root.createNestedObject("d");

  d["response"] = "OK";

  return publishPayload(root);
}

void callback(char* topic, byte * payload, unsigned int length) {
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

  uint8_t power = d["power"] == "on" ? POWER_ON : POWER_OFF;
  uint8_t mode = d["mode"] == "heat" ? MODE_HEAT : MODE_COOL;
  uint8_t fan = d["fan"];
  uint8_t temp = d["temp"];

  // Send the IR command
  heatpumpIR->send(irSender, power, mode, fan, temp, mode == MODE_HEAT ? VDIR_MDOWN : VDIR_UP, HDIR_MIDDLE);
  publishResponse();
}
