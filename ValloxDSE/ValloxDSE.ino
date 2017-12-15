#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <SoftwareSerial.h>
#include <ArduinoJson.h>
#include "xCredentials.h"

#define DEVICE_TYPE "VentillationMachine"
#define MESSAGE_LENGTH 6
#define COMMAND_LENGTH 12
#define NOT_SET -999;

const char publishTopic[] = "iot-2/evt/status/fmt/json";
const char manageTopic[] = "iotdevice-1/mgmt/manage";
const char updateTopic[] = "iotdm-1/device/update";

char server[] = ORG ".messaging.internetofthings.ibmcloud.com";
char authMethod[] = "use-token-auth";
char token[] = TOKEN;
char clientId[] = "d:" ORG ":" DEVICE_TYPE ":" DEVICE_ID;
void callback(char* topic, byte* payload, unsigned int payloadLength);

WiFiClient wifiClient;
PubSubClient client(server, 1883, callback, wifiClient);
SoftwareSerial mySerial(D1, D2); // RX, TX

typedef struct {
  int t_outside;
  int t_inside;
  int t_outbound;
  int t_inbound;
  int fan_speed;
} dse_result;
dse_result data;
int hash = 0;

int8_t temps[] = {
  -74, -70, -66, -62, -59, -56, -54, -52, -50, -48, // 0x00 - 0x09
  -47, -46, -44, -43, -42, -41, -40, -39, -38, -37, // 0x0a - 0x13
  -36, -35, -34, -33, -33, -32, -31, -30, -30, -29, // 0x14 - 0x1d
  -28, -28, -27, -27, -26, -25, -25, -24, -24, -23, // 0x1e - 0x27
  -23, -22, -22, -21, -21, -20, -20, -19, -19, -19, // 0x28 - 0x31
  -18, -18, -17, -17, -16, -16, -16, -15, -15, -14, // 0x32 - 0x3b
  -14, -14, -13, -13, -12, -12, -12, -11, -11, -11, // 0x3c - 0x45
  -10, -10, -9, -9, -9, -8, -8, -8, -7, -7,   // 0x46 - 0x4f
  -7, -6, -6, -6, -5, -5, -5, -4, -4, -4,    // 0x50 - 0x59
  -3, -3, -3, -2, -2, -2, -1, -1, -1, -1,    // 0x5a - 0x63
  0,  0,  0,  1,  1,  1,  2,  2,  2,  3,    // 0x64 - 0x6d
  3,  3,  4,  4,  4,  5,  5,  5,  5,  6,    // 0x6e - 0x77
  6,  6,  7,  7,  7,  8,  8,  8,  9,  9,    // 0x78 - 0x81
  9, 10, 10, 10, 11, 11, 11, 12, 12, 12,    // 0x82 - 0x8b
  13, 13, 13, 14, 14, 14, 15, 15, 15, 16,    // 0x8c - 0x95
  16, 16, 17, 17, 18, 18, 18, 19, 19, 19,    // 0x96 - 0x9f
  20, 20, 21, 21, 21, 22, 22, 22, 23, 23,    // 0xa0 - 0xa9
  24, 24, 24, 25, 25, 26, 26, 27, 27, 27,    // 0xaa - 0xb3
  28, 28, 29, 29, 30, 30, 31, 31, 32, 32,    // 0xb4 - 0xbd
  33, 33, 34, 34, 35, 35, 36, 36, 37, 37,    // 0xbe - 0xc7
  38, 38, 39, 40, 40, 41, 41, 42, 43, 43,    // 0xc8 - 0xd1
  44, 45, 45, 46, 47, 48, 48, 49, 50, 51,    // 0xd2 - 0xdb
  52, 53, 53, 54, 55, 56, 57, 59, 60, 61,    // 0xdc - 0xe5
  62, 63, 65, 66, 68, 69, 71, 73, 75, 77,    // 0xe6 - 0xef
  79, 81, 82, 86, 90, 93, 97, 100, 100, 100, // 0xf0 - 0xf9
  100, 100, 100, 100, 100, 100    // 0xfa - 0xff
};

byte speeds[][COMMAND_LENGTH] = {
  { 0x01, 0x11, 0x20, 0x29, 0x01, 0xDA, 0x01, 0x21, 0x10, 0x29, 0x01, 0xDA },   // speed 1
  { 0x01, 0x11, 0x20, 0x29, 0x03, 0x5E, 0x01, 0x21, 0x10, 0x29, 0x03, 0x5E },   // speed 2
  { 0x01, 0x11, 0x20, 0x29, 0x07, 0x62, 0x01, 0x21, 0x10, 0x29, 0x07, 0x62 },   // speed 3
  { 0x01, 0x11, 0x20, 0x29, 0x0F, 0x6A, 0x01, 0x21, 0x10, 0x29, 0x0F, 0x6A },   // speed 4
  { 0x01, 0x11, 0x20, 0x29, 0x1F, 0x7A, 0x01, 0x21, 0x10, 0x29, 0x1F, 0x7A },   // speed 5
  { 0x01, 0x11, 0x20, 0x29, 0x3F, 0x9A, 0x01, 0x21, 0x10, 0x29, 0x3F, 0x9A },   // speed 6
  { 0x01, 0x11, 0x20, 0x29, 0x7F, 0xDA, 0x01, 0x21, 0x10, 0x29, 0x7F, 0xDA },   // speed 7
  { 0x01, 0x11, 0x20, 0x29, 0xFF, 0x5A, 0x01, 0x21, 0x10, 0x29, 0xFF, 0x5A }    // speed 8
};

void setup() {
  Serial.begin(9600);

  wifiConnect();
  mqttConnect();

  // set the data rate for the SoftwareSerial port
  mySerial.begin(9600);

  // init data
  data.t_outside = NOT_SET;
  data.t_inside = NOT_SET;
  data.t_outbound = NOT_SET;
  data.t_inbound = NOT_SET;
  data.fan_speed = 3;
  hash = calculateHash();

  Serial.println("Setup done.");
}

void loop() {
  byte message[MESSAGE_LENGTH];
  while (readBusMessage(message)) {
    decodeMessage(message);
    Serial.print("Message received time="); Serial.println(millis());
  }

  int myHash = calculateHash();
  if (myHash != hash) {
    // data hash changed

    if (!client.loop()) {
      mqttConnect();
    }

    hash = myHash;
    prettyPrint();
    publishData();
  }
}

boolean readBusMessage(byte message[]) {
  if (mySerial.available() > 2) {
    message[0] = mySerial.read();
    message[1] = mySerial.read();
    message[2] = mySerial.read();

    if (message[0] == 0x01 && message[1] == 0x11 && message[2] == 0x20) {
      int i = 3;
      while (i < MESSAGE_LENGTH) {
        if (mySerial.available()) {
          message[i++] = mySerial.read();
        }
        delay(10);
      }

      return true;
    }
  }

  return false;
}

boolean decodeMessage(byte message[]) {
  if (message[0] == 0x01 &&
      message[1] == 0x11 &&
      message[2] == 0x20) {
    byte c = message[3];
    byte t = message[4];

    if (c == 0x58) {
      data.t_outside = decodeTemperature(t);
    } else if (c == 0x5c) {
      data.t_outbound = decodeTemperature(t);
    } else if (c == 0x5a) {
      data.t_inside = decodeTemperature(t);
    } else if (c == 0x5b) {
      data.t_inbound = decodeTemperature(t);
    }
  }
}

void prettyPrint() {
  Serial.print("Inside ait T="); Serial.print(data.t_inside); Serial.print(", ");
  Serial.print("Outside air T="); Serial.print(data.t_outside); Serial.print(", ");
  Serial.print("Inbound air T="); Serial.print(data.t_inbound); Serial.print(", ");
  Serial.print("Outbound air T="); Serial.print(data.t_outbound);
  Serial.println();
}

void prettyPrint(byte message[]) {
  Serial.print(message[0], HEX); Serial.print(" ");
  Serial.print(message[1], HEX); Serial.print(" ");
  Serial.print(message[2], HEX); Serial.print(" ");
  Serial.print(message[3], HEX); Serial.print(" ");
  Serial.print(message[4], HEX); Serial.print(" ");
  Serial.print(message[5], HEX); Serial.print(" ");
  Serial.println();
}

int decodeTemperature(byte b) {
  int i = (int)b;
  return temps[i];
}

void setSpeed(int speed) {
  if (speed > 0 && speed < 9) {
    for (int i = 0; i < COMMAND_LENGTH; i++) {
      mySerial.write(speeds[speed][i]);
      delay(10);
    }

    data.fan_speed = speed;
  }
}

int calculateHash() {
  int hash = data.t_outside * 1 +
             data.t_inside * 10 +
             data.t_outbound * 100 +
             data.t_inbound * 1000 +
             data.fan_speed * 10000;

  return hash;
}

void wifiConnect() {
  Serial.print("Connecting to "); Serial.print(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
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
  }

  initManagedDevice();
}

boolean isSet(int value) {
  return value != NOT_SET;
}

void publishData() {
  StaticJsonBuffer<300> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  JsonObject& d = root.createNestedObject("d");

  if (isSet(data.t_inside)) {
    d["T_IN"] = data.t_inside;
  }
  
  if (isSet(data.t_outside)) {
    d["T_OUT"] = data.t_outside;
  }
  
  if (isSet(data.t_inbound)) {
    d["T_INB"] = data.t_inbound;
  }
  
  if (isSet(data.t_outbound)) {
    d["T_OUTB"] = data.t_outbound;
  }
  
  if (isSet(data.fan_speed)) {
    d["SPEED"] = data.fan_speed;
  }
  
  char buff[300];
  root.printTo(buff, sizeof(buff));

  if (client.publish(publishTopic, buff)) {
    Serial.println("Publish OK");
  } else {
    Serial.println("Publish FAILED");
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("callback invoked for topic: "); Serial.println(topic);

  if (strcmp (updateTopic, topic) == 0) {
    handleUpdate(payload);
  }
}

void handleUpdate(byte* payload) {
  StaticJsonBuffer<300> jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject((char*)payload);
  if (!root.success()) {
    Serial.println("handleUpdate: payload parse FAILED");
    return;
  }
  Serial.println("handleUpdate payload:"); root.prettyPrintTo(Serial); Serial.println();

  JsonObject& d = root["d"];
  JsonArray& fields = d["fields"];
  for (JsonArray::iterator it = fields.begin(); it != fields.end(); ++it) {
    JsonObject& field = *it;
    const char* fieldName = field["field"];
    if (strcmp (fieldName, "metadata") == 0) {
      JsonObject& fieldValue = field["value"];
      if (fieldValue.containsKey("speed")) {
        int speed = fieldValue["speed"];
        setSpeed(speed);
        Serial.print("speed:"); Serial.println(data.fan_speed);
      }
    }
  }
}

void initManagedDevice() {
  if (client.subscribe(updateTopic)) {
    Serial.println("subscribe to update OK");
  } else {
    Serial.println("subscribe to update FAILED");
  }

  StaticJsonBuffer<300> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  JsonObject& d = root.createNestedObject("d");
  JsonObject& metadata = d.createNestedObject("metadata");
  metadata["speed"] = data.fan_speed;
  JsonObject& supports = d.createNestedObject("supports");
  supports["deviceActions"] = true;

  char buff[300];
  root.printTo(buff, sizeof(buff));
  Serial.println("publishing device metadata:"); Serial.println(buff);
  if (client.publish(manageTopic, buff)) {
    Serial.println("device Publish ok");
  } else {
    Serial.print("device Publish failed:");
  }
}
