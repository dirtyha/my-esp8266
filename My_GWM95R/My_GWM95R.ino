#include <ESP8266WiFi.h>
#include <ModbusMaster.h>
#include <PubSubClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <SoftwareSerial.h>
#include "xCredentials.h"

const char publishTopic[] = "events/" DEVICE_ID; // publish events here
const char cmdTopic[] = "cmd/" DEVICE_ID;        // subscribe for commands here
const char AWS_endpoint[] = AWS_PREFIX ".iot.eu-west-1.amazonaws.com";
void callback(char* topic, byte* payload, unsigned int payloadLength);
const char clientId[] = "ESP8266-" DEVICE_ID;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");
WiFiClientSecure espClient;
PubSubClient client(AWS_endpoint, 8883, espClient);
SoftwareSerial sws(4, 0);
ModbusMaster node;

long lastPublishMillis;

typedef struct {
  float ta;
  float td;
  float rh;
  float a;
  int co2;
} t_wt_data;
t_wt_data data;

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
  // configure Serial port 0 and modbus slave 1

  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  sws.begin(9600, SWSERIAL_8N2);
  node.begin(1, sws);
  publishData();

  client.disconnect();
  WiFi.disconnect();

  Serial.println("Done");

  // sleep 10 minutes
  ESP.deepSleep(600e6);
}

void loop() {
}


boolean poll() {
  // slave: read 3 16-bit registers starting at register 2 to RX buffer
  uint8_t result = node.readHoldingRegisters(256, 10);

  // do something with data if read is successful
  if (result == node.ku8MBSuccess)
  {
    data.co2 = node.getResponseBuffer(0);
    data.rh = node.getResponseBuffer(1) * 0.01f;
    data.ta = node.getResponseBuffer(2) * 0.01f;
    data.td = node.getResponseBuffer(3) * 0.01f;
    data.a = node.getResponseBuffer(7) * 0.01f;
  } else {
    Serial.println("Failed to read modbus.");
    return false;
  }

  return true;
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

void publishData() {
  if (poll()) {
    // construct a JSON response
    String json = "{\"d\":{";
    json += "\"TA\":";
    json += data.ta;
    json += ",\"Td\":";
    json += data.td;
    json += ",\"RH\":";
    json += data.rh;
    json += ",\"a\":";
    json += data.a;
    json += ",\"CO2\":";
    json += data.co2;
    json += "}}";

    if (client.publish(publishTopic, (char*) json.c_str())) {
      Serial.println("Publish OK");
    } else {
      Serial.println("Publish FAILED");
    }
  }
}
