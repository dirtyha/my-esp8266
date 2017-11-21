#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "DHT.h"
#include "xADC.h"
#include "xCredentials.h"

#define DHTPIN 4     // what digital pin the DHT22 is conected to
#define DHTTYPE DHT22   // there are multiple kinds of DHT sensors
#define DEVICE_TYPE "DHT22"

const char publishTopic[] = "iot-2/evt/status/fmt/json";

char server[] = ORG ".messaging.internetofthings.ibmcloud.com";
char authMethod[] = "use-token-auth";
char token[] = TOKEN;
char clientId[] = "d:" ORG ":" DEVICE_TYPE ":" DEVICE_ID;

WiFiClient wifiClient;
PubSubClient client(server, 1883, wifiClient);
DHT dht(DHTPIN, DHTTYPE);

typedef struct {
  float ta;
  float td;
  float rh;
  float u;
} dht_result;

void setup() {
  Serial.begin(9600);
  while (!Serial) {}

  wifiConnect();
  mqttConnect();

  publishData();

  mqttDisconnect();
  wifiDisconnect();

  // sleep 10 minutes
  ESP.deepSleep(600e6);
}

/*
  void setup() {
  Serial.begin(9600);
  Serial.setTimeout(2000);
  while (!Serial) {}

  wifiConnect();
  mqttConnect();

  Serial.println("Setup done.");
  }
*/

void loop() {
}

/*
long lastPublishMillis = 0;
void loop() {
  if (millis() - lastPublishMillis > 60000) {
    publishData();
    lastPublishMillis = millis();
  }

  if (!client.loop()) {
    mqttConnect();
  }
}
*/

void wifiConnect() {
//  Serial.print("Connecting to "); Serial.print(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
//    Serial.print(".");
  }
//  Serial.print("WiFi connected, IP address: "); Serial.println(WiFi.localIP());
}

void wifiDisconnect() {
  if (WiFi.status() == WL_CONNECTED) {
    WiFi.disconnect();
  }
}

void mqttConnect() {
  if (!!!client.connected()) {
//    Serial.print("Reconnecting MQTT client to "); Serial.println(server);
    while (!!!client.connect(clientId, authMethod, token)) {
//      Serial.print(".");
      delay(500);
    }
//    Serial.println();
  }
}

void mqttDisconnect() {
  if (client.connected()) {
    client.disconnect();
  }
}

void publishData() {
  dht_result* result = poll();
  if (result != NULL) {
    // construct a JSON response
    String json = "{\"d\":{";
    json += "\"TA\":";
    json += result->ta;
    json += ",\"Td\":";
    json += result->td;
    json += ",\"RH\":";
    json += result->rh;
    json += ",\"U\":";
    json += result->u;
    json += "}}";

    if (client.publish(publishTopic, (char*) json.c_str())) {
//      Serial.println("Publish OK");
    } else {
//      Serial.println("Publish FAILED");
    }
  }
}

dht_result* poll() {
  int count = 10;
  while (count-- > 0) {
    float rh = dht.readHumidity();
    float ta = dht.readTemperature();

//    Serial.print("TA = "); Serial.println(ta);
//    Serial.print("RH = "); Serial.println(rh);

    if (isnan(rh) || isnan(ta)) {
//      Serial.println("Failed to read from DHT sensor!");
      delay(2000);
    } else {
      dht_result* ret = (dht_result*)malloc(sizeof(dht_result));
      if (ret != NULL) {

        ret->rh = rh;
        ret->ta = ta;
        ret->u = ESP.getVcc() / 1000.0;
        ret->td = calculateTd(rh, ta);

        return ret;
      } else {
//        Serial.println("Malloc failed.");
      }
    }
  }

  return NULL;
}

float calculateTd(float rh, float ta) {
  return 243.04 * (log(rh / 100.0) + ((17.625 * ta) / (243.04 + ta))) / (17.625 - log(rh / 100.0) - ((17.625 * ta) / (243.04 + ta)));
}

