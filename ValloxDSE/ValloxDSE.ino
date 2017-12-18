// Vallox Digit SE monitoring and control for ESP8266
// requires RS485 serial line adapter between ESP8266 <-> DigitSE
// and account in Watson IoT platform

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <SoftwareSerial.h>
#include <ArduinoJson.h>
#include <CRC32.h>
#include "xCredentials.h"
#include "xVallox.h"

#define DEVICE_TYPE "VentillationMachine"
#define JSON_BUFFER_LENGTH 300

const char publishTopic[] = "iot-2/evt/status/fmt/json";          // publish measurements here
const char updateTopic[] = "iot-2/cmd/update/fmt/json";           // subscribe for update command

// watson iot stuff
char server[] = ORG ".messaging.internetofthings.ibmcloud.com";
char authMethod[] = "use-token-auth";
char token[] = TOKEN;
char clientId[] = "d:" ORG ":" DEVICE_TYPE ":" DEVICE_ID;

WiFiClient wifiClient;
void myCallback(char* topic, byte* payload, unsigned int payloadLength);
PubSubClient client(server, 1883, myCallback, wifiClient);
Vallox vx(D1, D2, true);
CRC32 crc;
uint32_t oldHash;

void setup() {
  Serial.begin(9600);

  wifiConnect();
  vx_data* data = vx.init();
  oldHash = crc.calculate(data, sizeof(vx_data));
  mqttConnect();

  Serial.println("Setup done.");
}

void loop() {

  // loop VX messages
  vx_data* data = vx.loop();

  // check that we are connected
  if (!client.loop()) {
    mqttConnect();
  }

  uint32_t newHash = crc.calculate(data, sizeof(vx_data));
  if (newHash != oldHash) {
    // data hash changed
    publishData(data);
    oldHash = newHash;
    prettyPrint(data);
  }
}

void prettyPrint(vx_data* data) {
  Serial.print("Inside ait T="); Serial.print(data->t_inside); Serial.print(", ");
  Serial.print("Outside air T="); Serial.print(data->t_outside); Serial.print(", ");
  Serial.print("Inbound air T="); Serial.print(data->t_inbound); Serial.print(", ");
  Serial.print("Outbound air T="); Serial.print(data->t_outbound); Serial.print(", ");
  Serial.print("Fan speed="); Serial.print(data->fan_speed);
  Serial.println();
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
    while (!!!client.connect(clientId, authMethod, token)) {
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

void publishData(vx_data* data) {
  StaticJsonBuffer<JSON_BUFFER_LENGTH> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  JsonObject& d = root.createNestedObject("d");

  if (vxIsSet(data->t_inside)) {
    d["T_IN"] = data->t_inside;
  }

  if (vxIsSet(data->t_outside)) {
    d["T_OUT"] = data->t_outside;
  }

  if (vxIsSet(data->t_inbound)) {
    d["T_INB"] = data->t_inbound;
  }

  if (vxIsSet(data->t_outbound)) {
    d["T_OUTB"] = data->t_outbound;
  }

  if (vxIsSet(data->fan_speed)) {
    d["SPEED"] = data->fan_speed;
  }

  if (vxIsSet(data->is_on)) {
    d["ON"] = data->is_on;
  }

  char buff[JSON_BUFFER_LENGTH];
  root.printTo(buff, sizeof(buff));

  if (client.publish(publishTopic, buff)) {
    Serial.println("Publish OK");
  } else {
    Serial.println("Publish FAILED");
  }
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
  if (d.containsKey("speed")) {
    int speed = d["speed"];
    vx.setFanSpeed(speed);
  }
}


