#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Servo.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "xCredentials.h"

#define INPUT_BUFFER_LENGTH 300
#define OUTPUT_BUFFER_LENGTH 300
#define DEBUG 1

const char publishTopic[] = "events"; // publish events here
const char cmdTopic[] = "cmd";        // subscribe for commands here
const char AWS_endpoint[] = AWS_PREFIX ".iot.eu-west-1.amazonaws.com";
void callback(char* topic, byte* payload, unsigned int payloadLength);

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");
WiFiClientSecure espClient;
PubSubClient client(AWS_endpoint, 8883, callback, espClient);

Servo myservo; //initialize a servo object
int angle = 0;
char output_buffer[OUTPUT_BUFFER_LENGTH];

void setup()
{
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

  myservo.attach(4); // connect the servo at pin D2
  myservo.write(0);
  
  if (DEBUG) {
    Serial.println("Setup done");
  }
}

void loop()
{
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
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
    if (client.connect("ESPthing")) {
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

void callback(char* topic, byte * payload, unsigned int length) {
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
      publish(angle2temp(myservo.read()));
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
