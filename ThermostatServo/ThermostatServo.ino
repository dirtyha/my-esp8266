#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Servo.h>
#include "xCredentials.h"

#define DEVICE_TYPE "Servo"

#define INPUT_BUFFER_LENGTH 300
#define OUTPUT_BUFFER_LENGTH 300
#define DEBUG 1

const char publishTopic[] = "events/" DEVICE_TYPE "/" DEVICE_ID;               // publish events here
const char cmdTopic[] = "cmd/" DEVICE_TYPE "/" DEVICE_ID;             // subscribe for commands here
const char server[] = "myhomeat.cloud";
const char authMethod[] = "use-token-auth";
const char token[] = TOKEN;
const char clientId[] = "d:" DEVICE_TYPE ":" DEVICE_ID;

WiFiClient wifiClient;
void myCallback(char* topic, byte* payload, unsigned int payloadLength);
PubSubClient client(server, 1883, myCallback, wifiClient);

Servo myservo; //initialize a servo object
int angle = 0;
char output_buffer[OUTPUT_BUFFER_LENGTH];

void setup()
{
  Serial.begin(57600);
  delay(1000);

  wifiConnect();
  mqttConnect();

  myservo.attach(4); // connect the servo at pin D2
  myservo.write(0);
  
  if (DEBUG) {
    Serial.println("Setup done");
  }
}

void loop()
{
  // check that we are connected
  if (!client.loop()) {
    mqttConnect();
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

void mqttConnect() {
  if (!!!client.connected()) {
    if (DEBUG) {
      Serial.print("Reconnecting MQTT client to "); Serial.println(server);
    }
    int count = 20;
    while (count-- > 0 && !!!client.connect(clientId, authMethod, token)) {
      if (DEBUG) {
        Serial.print(".");
      }
      delay(500);
    }
    if (DEBUG) {
      Serial.println();
    }

    if (client.subscribe(cmdTopic, 1)) {
      if (DEBUG) {
        Serial.println("Subscribe to update OK");
      }
    } else {
      if (DEBUG) {
        Serial.println("Subscribe to update FAILED");
      }
    }
  }
}

void myCallback(char* topic, byte * payload, unsigned int length) {
  if (DEBUG) {
    Serial.print("Callback invoked for topic: "); Serial.println(topic);
  }

  if (strcmp (cmdTopic, topic) == 0) {
    handleUpdate(payload);
  }
}

void handleUpdate(byte * payload) {
  StaticJsonBuffer<INPUT_BUFFER_LENGTH> jsonBuffer;
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

  if (d.containsKey("cmd")) {
    String cmd = d["cmd"];
    if (cmd == "get") {
      publish(angle2temp(myservo.read()));
    } else if (cmd == "set") {
      int temp = d["temp"];
      myservo.write(temp2angle(temp));
    }
  }
}

boolean publishPayload(JsonObject& root) {
  boolean ret = true;

  if (DEBUG) {
    Serial.println("Publish payload:"); root.prettyPrintTo(Serial); Serial.println();
  }

  root.printTo(output_buffer, OUTPUT_BUFFER_LENGTH);

  if (client.publish(publishTopic, output_buffer)) {
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

boolean publish(int angle) {
  StaticJsonBuffer<OUTPUT_BUFFER_LENGTH> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  JsonObject& d = root.createNestedObject("d");

  d["temp"] = angle;

  return publishPayload(root);
}

// scale 1  => 15 C
// scale 10 => 85 C
// servo 0-180 degrees => scale 2-8

int angle2temp(int angle) {
  if (angle < 0) {
    return 23;
  }

  if (angle > 180) {
    return 69;
  }

  return round(22.777 + (angle * 7.777) / 30.0);
}

int temp2angle(int temp) {
  if (temp < 23) {
    return 0;
  }

  if (temp > 69) {
    return 180;
  }

  return round((temp - 22.777) * 30.0 / 7.777);
}
