#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "xCredentials.h"

#define DEVICE_TYPE "PulseCounter"

char server[] = "myhomeat.cloud";
char authMethod[] = "use-token-auth";
char token[] = TOKEN;
char clientId[] = "d:" DEVICE_TYPE ":" DEVICE_ID;
const char publishTopic[] = "events/" DEVICE_TYPE "/" DEVICE_ID;

WiFiClient wifiClient;
PubSubClient client(server, 1883, wifiClient);

const int led_pin = D1;

const int numReadings = 30;
int readings[numReadings];      // the readings from the analog input
int readIndex = 0;              // the index of the current reading
int total = 0;                  // the running total
int minValue = 0;
int maxValue = 0;

int counter = 0;
unsigned long lastRead = 0;
unsigned long lastPrint = 0;
unsigned long lastSent = 0;
boolean signalUp = false;

void setup() {
  // initialize serial communication with computer:
  Serial.begin(57600);
  while (!Serial) {}

  pinMode(led_pin, OUTPUT);
  digitalWrite(led_pin, HIGH);

  wifiConnect();
  mqttConnect();

  // initialize all the readings to 0:
  for (int thisReading = 0; thisReading < numReadings; thisReading++) {
    readings[thisReading] = 0;
  }
  resetMinMax();
}

void loop() {
  unsigned long now = millis();

  if (now - lastRead > 1) {
    // read data from analog input
    lastRead = now;

    // get reading
    int value = readAverage(A0);

    if (value < minValue) {
      minValue = value;
    } else if (value > maxValue) {
      maxValue = value;
    }

    int delta = maxValue - minValue;

    if (delta > 5 && delta < 20) {
      // only count pulses if the delta is between 5 and 20
      int highThreshold = maxValue - delta * 0.5 + 1;
      int lowThreshold = highThreshold - 1;
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
    // reconnect and send data to Watson IoT
    if (!client.loop()) {
      mqttConnect();
    }
    
    lastSent = now;
    if(publishData()) {
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
    int retryCount = 60;
    while (!!!client.connect(clientId, authMethod, token) && --retryCount > 0) {
      Serial.print(".");
      delay(500);
    }
    Serial.println();
  }
}

boolean publishData() {
  
  // construct a JSON response
  String json = "{\"d\":{";
  json += "\"WP\":";
  json += counter;
  json += "}}";

  if (client.publish(publishTopic, (char*) json.c_str())) {
    Serial.println("Publish OK");
    return true;
  } else {
    Serial.println("Publish FAILED");
    return false;
  }
}

