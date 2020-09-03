#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <SDS011.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "xCredentials.h"

#define JSON_BUFFER_LENGTH 100
#define DEBUG 1

float pm10, pm10_sum;
int pm10_count;
float pm25, pm25_sum;
int pm25_count;

const char publishTopic[] = "events/" DEVICE_ID; // publish events here
const char AWS_endpoint[] = AWS_PREFIX ".iot.eu-west-1.amazonaws.com";
const char clientId[] = "ESP8266-" DEVICE_ID;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");
WiFiClientSecure espClient;
PubSubClient client(AWS_endpoint, 8883, espClient);
SDS011 sds;
const int led_pin = 0;

unsigned long lastAwake = 0;
unsigned long lastCheck = 0;
boolean isAwake = false;
boolean isReady = false;

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

  sds.begin(5, 4);

  pinMode(led_pin, OUTPUT);
  digitalWrite(led_pin, LOW);

  sleepSds();
  Serial.println("Setup done.");
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  unsigned long now = millis();

  if (!isAwake && (now - lastAwake > 8.5 * 60 * 1000)) {
      wakeupSds();
  }

  if (!isReady && (now - lastAwake > 9 * 60 * 1000)) {
    isReady = true;
    digitalWrite(led_pin, HIGH);
  }

  if (now - lastAwake > 10 * 60 * 1000) {
    lastAwake = now;
    isReady = false;
    sleepSds();
    digitalWrite(led_pin, LOW);

    if (publishData()) {
      pm10_sum = 0.0;
      pm10_count = 0;
      pm25_sum = 0.0;
      pm25_count = 0;
    }
  }

  if (isReady && (now - lastCheck > 2000)) {
    lastCheck = now;

    if (!sds.read(&pm25, &pm10)) {
      Serial.println("PM2.5: " + String(pm25));
      Serial.println("P10:  " + String(pm10));

      pm10_sum += pm10;
      pm10_count++;

      pm25_sum += pm25;
      pm25_count++;
    } else {
      Serial.println("Failed to read SDS.");
    }
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

boolean publishData() {
  boolean ret = true;

  float pm10_avg = pm10_sum / pm10_count;
  float pm25_avg = pm25_sum / pm25_count;

  StaticJsonBuffer<JSON_BUFFER_LENGTH> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  JsonObject& d = root.createNestedObject("d");

  d["PM25"] = pm25_avg;
  d["PM10"] = pm10_avg;

  Serial.println("Publish payload:"); root.prettyPrintTo(Serial); Serial.println();

  char buff[JSON_BUFFER_LENGTH];
  root.printTo(buff, JSON_BUFFER_LENGTH);

  if (!client.publish(publishTopic, buff)) {
    Serial.println("Publish FAILED");
    ret = false;
  }

  return ret;
}

void sleepSds() {
  sds.sleep();
  isAwake = false;
  Serial.println("SDS011 is sleeping.");
}

void wakeupSds() {
  sds.wakeup();
  isAwake = true;
  Serial.println("SDS011 is awake.");
}
