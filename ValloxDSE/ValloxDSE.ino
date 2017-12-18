// Vallox Digit SE monitoring and control for ESP8266
// requires RS485 serial line adapter between ESP8266 <-> DigitSE
// and account in Watson IoT platform

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <SoftwareSerial.h>
#include <ArduinoJson.h>
#include "xCredentials.h"
#include "xValloxDSE.h"

#define DEVICE_TYPE "VentillationMachine"
#define NOT_SET -999;
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
SoftwareSerial mySerial(D1, D2); // RX, TX connected to RS485 adapter

// measured data from ValloxDSE
struct {
  int t_outside;
  int t_inside;
  int t_outbound;
  int t_inbound;
  int fan_speed;
} data;
int oldHash; // used to check if data has changed

void setup() {
  Serial.begin(9600);

  wifiConnect();

  // set the data rate for the SoftwareSerial port
  mySerial.begin(9600);

  // init data
  data.t_outside =  NOT_SET;   // read only
  data.t_inside =   NOT_SET;    // read only
  data.t_outbound = NOT_SET;  // read only
  data.t_inbound =  NOT_SET;   // read only
  data.fan_speed =  3;         // write only
  oldHash = calculateHash();

  mqttConnect();
  vxPollFanSpeed();

  Serial.println("Setup done.");
}

void loop() {
  byte message[VX_MSG_LENGTH];

  // read and decode all available messages
  while (vxReadMessage(message)) {
    vxDecodeMessage(message);
    prettyPrint(message);
  }

  // check that we are connected
  if (!client.loop()) {
    mqttConnect();
  }

  int newHash = calculateHash();
  if (newHash != oldHash) {
    // data hash changed
    publishData();
    oldHash = newHash;
    prettyPrint();
  }
}

void prettyPrint() {
  Serial.print("Inside ait T="); Serial.print(data.t_inside); Serial.print(", ");
  Serial.print("Outside air T="); Serial.print(data.t_outside); Serial.print(", ");
  Serial.print("Inbound air T="); Serial.print(data.t_inbound); Serial.print(", ");
  Serial.print("Outbound air T="); Serial.print(data.t_outbound); Serial.print(", ");
  Serial.print("Fan speed="); Serial.print(data.fan_speed);
  Serial.println();
}

void prettyPrint(byte message[]) {
  for (int i = 0; i < VX_MSG_LENGTH; i++) {
    Serial.print(message[i], HEX); Serial.print(" ");
  }
  Serial.println();
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

boolean isSet(int value) {
  return value != NOT_SET;
}

void publishData() {
  StaticJsonBuffer<JSON_BUFFER_LENGTH> jsonBuffer;
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
    vxSetFanSpeed(speed);
  }
}

void vxSetFanSpeed(int speed) {
  vxSetVariable(VX_VARIABLE_FAN_SPEED, vxFanSpeed2Hex(speed));
}

void vxPollFanSpeed() {
  vxPollVariable(VX_VARIABLE_FAN_SPEED);
}

void vxPollStatus() {
  vxPollVariable(VX_VARIABLE_STATUS);
}

void vxPollService() {
  vxPollVariable(VX_VARIABLE_SERVICE_PERIOD);
  vxPollVariable(VX_VARIABLE_SERVICE_COUNTER);
}

// set variable value in mainboards and panels
void vxSetVariable(byte variable, byte value) {
  byte message[VX_MSG_LENGTH];

  message[0] = VX_MSG_DOMAIN;
  message[1] = VX_MSG_PANEL_1;
  message[2] = VX_MSG_MAINBOARDS;
  message[3] = variable;
  message[4] = value;
  message[5] = vxCalculateCheckSum(message);

  // send to mainboards
  for (int i = 0; i < VX_MSG_LENGTH; i++) {
    mySerial.write(message[i]);
  }

  message[1] = VX_MSG_MAINBOARD_1;
  message[2] = VX_MSG_PANELS;
  message[5] = vxCalculateCheckSum(message);

  // send to panels
  for (int i = 0; i < VX_MSG_LENGTH; i++) {
    mySerial.write(message[i]);
  }
}

// poll variable value in mainboards
// the value will be received by the main loop
void vxPollVariable(byte variable) {
  byte message[VX_MSG_LENGTH];

  message[0] = VX_MSG_DOMAIN;
  message[1] = VX_MSG_PANEL_1;
  message[2] = VX_MSG_MAINBOARD_1;
  message[3] = VX_MSG_POLL_BYTE;
  message[4] = variable;
  message[5] = vxCalculateCheckSum(message);

  for (int i = 0; i < VX_MSG_LENGTH; i++) {
    mySerial.write(message[i]);
  }
}

// reads one full message
boolean vxReadMessage(byte message[]) {
  boolean ret = false; // true = new message has been received, false = no message

  if (mySerial.available() >= VX_MSG_LENGTH) {
    message[0] = mySerial.read();

    if (message[0] == VX_MSG_DOMAIN) {
      message[1] = mySerial.read();
      message[2] = mySerial.read();

      // accept messages from mainboard to panel 1 or all panels
      // accept messages from panel to mainboard 1 or all mainboards
      if ((message[1] == VX_MSG_MAINBOARD_1 || message[1] == VX_MSG_PANEL_1) &&
          (message[2] == VX_MSG_PANELS || message[2] == VX_MSG_PANEL_1 ||
           message[2] == VX_MSG_MAINBOARD_1 || message[2] == VX_MSG_MAINBOARDS)) {
        int i = 3;
        // read the rest of the message
        while (i < VX_MSG_LENGTH) {
          message[i++] = mySerial.read();
        }

        ret = true;
      }
    }
  }

  return ret;
}

void vxDecodeMessage(byte message[]) {
  // decode variable in message
  byte variable = message[3];
  byte value = message[4];

  if (variable == VX_VARIABLE_T_OUTSIDE) {
    data.t_outside = vxNtc2Cel(value);
  } else if (variable == VX_VARIABLE_T_OUTBOUND) {
    data.t_outbound = vxNtc2Cel(value);
  } else if (variable == VX_VARIABLE_T_INSIDE) {
    data.t_inside = vxNtc2Cel(value);
  } else if (variable == VX_VARIABLE_T_INBOUND) {
    data.t_inbound = vxNtc2Cel(value);
  } else if (variable == VX_VARIABLE_FAN_SPEED) {
    data.fan_speed = vxHex2FanSpeed(value);
  } else if (variable == VX_VARIABLE_STATUS) {
    // TODO
  } else if (variable == VX_VARIABLE_SERVICE_PERIOD) {
    // TODO
  } else if (variable == VX_VARIABLE_SERVICE_COUNTER) {
    // TODO
  } else if (variable == VX_VARIABLE_RH) {
    // TODO
  } else {
    // variable not recognized
  }
}

// calculate VX message checksum
byte vxCalculateCheckSum(const byte message[]) {
  byte ret = 0x00;
  for (int i = 0; i < VX_MSG_LENGTH - 1; i++) {
    ret += message[i];
  }

  return ret;
}

int vxNtc2Cel(byte b) {
  int i = (int)b;
  return vxTemps[i];
}

byte vxFanSpeed2Hex(int val) {
  return vxFanSpeeds[val - 1];
}

int vxHex2FanSpeed(byte b) {
  for (int i = 0; i < sizeof(vxFanSpeeds); i++) {
    if (vxFanSpeeds[i] == b) {
      return i + 1;
    }
  }

  return 0;
}


