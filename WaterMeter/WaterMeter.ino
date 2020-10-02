#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "xCredentials.h"

#define DEBUG 1

const char publishTopic[] = "events/" DEVICE_ID; // publish events here
const char AWS_endpoint[] = AWS_PREFIX ".iot.eu-west-1.amazonaws.com";
const char clientId[] = "ESP8266-" DEVICE_ID;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");
WiFiClientSecure espClient;
PubSubClient client(AWS_endpoint, 8883, espClient);

const int led_pin = 5;

const int numReadings = 20;
int readings[numReadings];      // the readings from the analog input
int readIndex = 0;              // the index of the current reading
int total = 0;                  // the running total
int minValue = 0;
int maxValue = 0;

int counter = 0;
unsigned long lastPrint = 0;
unsigned long lastRead = 0;
unsigned long lastSent = 0;
boolean signalUp = false;

File ca;
File cert;
File private_key;

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);

  if (!SPIFFS.begin()) {
    Serial.println("Failed to mount file system");
    return;
  }

  Serial.print("Heap: "); Serial.println(ESP.getFreeHeap());

  // Load certificate file
  cert = SPIFFS.open("/cert.der", "r");
  if (!cert) {
    Serial.println("Failed to open cert file");
  }
  else
    Serial.println("Success to open cert file");

  delay(1000);

  // Load private key file
  private_key = SPIFFS.open("/private.der", "r");
  if (!private_key) {
    Serial.println("Failed to open private cert file");
  }
  else
    Serial.println("Success to open private cert file");

  delay(1000);

  // Load CA file
  ca = SPIFFS.open("/ca.der", "r");
  if (!ca) {
    Serial.println("Failed to open ca ");
  }
  else
    Serial.println("Success to open ca");

  WiFi.disconnect();

  pinMode(A0, INPUT);
  pinMode(led_pin, OUTPUT);
  digitalWrite(led_pin, HIGH);

  // initialize all the readings to 0:
  for (int thisReading = 0; thisReading < numReadings; thisReading++) {
    readings[thisReading] = 0;
  }
  resetMinMax();

  Serial.println("Setup done.");
}

void loop() {
  unsigned long now = millis();

  if (now - lastRead > 1) {
    lastRead = now;
    
    // get reading
    int value = readAverage(A0);

    if (value < minValue) {
      minValue = value;
    } else if (value > maxValue) {
      maxValue = value;
    }

    int delta = maxValue - minValue;
    int avg = (maxValue + minValue) / 2;

    if (delta > 5 && delta < 20) {
      // only count pulses if the delta is between 5 and 20
      //      int highThreshold = maxValue - delta * 0.5 + 1;
      //      int lowThreshold = highThreshold - 1;
      int highThreshold = avg + 1.0;
      int lowThreshold = avg;
      if (value > highThreshold && signalUp == false) {
        digitalWrite(led_pin, LOW);
        signalUp = true;
      } else if (value < lowThreshold && signalUp == true) {
        counter++;
        digitalWrite(led_pin, HIGH);
        signalUp = false;
      }
    } else {
      if (delta > 20) {
        // induced noise has made delta too big
        resetMinMax();
      }
    }
  }

  if (now - lastPrint > 5000) {
    // print status in console
    lastPrint = now;
    Serial.print("counter = "); Serial.print(counter);
    Serial.print(", min = "); Serial.print(minValue);
    Serial.print(", max = "); Serial.println(maxValue);
  }

  if (now - lastSent > 600000) {
    lastSent = now;
    if (publishData()) {
      counter = 0;
    }
  }
}

void resetMinMax() {
  minValue = 1024;
  maxValue = 0;
}

int readAverage(int pin) {
  total = total - readings[readIndex];
  readings[readIndex] = analogRead(pin);
  total = total + readings[readIndex];
  readIndex = readIndex + 1;

  if (readIndex >= numReadings) {
    readIndex = 0;
  }

  return total / numReadings;
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
  boolean ret = false;

  setup_wifi();

  if (espClient.loadCertificate(cert))
    Serial.println("cert loaded");
  else
    Serial.println("cert not loaded");

  if (espClient.loadPrivateKey(private_key))
    Serial.println("private key loaded");
  else
    Serial.println("private key not loaded");

  if (espClient.loadCACert(ca))
    Serial.println("ca loaded");
  else
    Serial.println("ca failed");

  Serial.print("Heap: "); Serial.println(ESP.getFreeHeap());

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // construct a JSON response
  String json = "{\"d\":{";
  json += "\"WP\":";
  json += counter;
  json += "}}";

  if (client.publish(publishTopic, (char*) json.c_str())) {
    Serial.println("Publish OK");
    ret = true;
  } else {
    Serial.println("Publish FAILED");
  }

  WiFi.disconnect();

  return ret;
}
