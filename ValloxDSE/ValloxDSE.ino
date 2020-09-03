// Vallox Digit SE monitoring and control for ESP8266
// requires RS485 serial line adapter between ESP8266 <-> DigitSE
// NOTE: MQTT message size is over 128 bytes. 
// You must increase MQTT_MAX_PACKET_SIZE to 256 in PubSubClient.h

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Vallox.h>
#include "xCredentials.h" 

#define JSON_BUFFER_LENGTH 350
#define DEBUG 1

const char publishTopic[] = "events/" DEVICE_ID; // publish events here
const char cmdTopic[] = "cmd/" DEVICE_ID;        // subscribe for commands here
const char AWS_endpoint[] = AWS_PREFIX ".iot.eu-west-1.amazonaws.com";
void callback(char* topic, byte* payload, unsigned int payloadLength);
const char clientId[] = "ESP8266-" DEVICE_ID;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");
WiFiClientSecure espClient;
PubSubClient client(AWS_endpoint, 8883, callback, espClient);
Vallox vx(5, 4, DEBUG);
unsigned long lastUpdated = 0;

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

  vx.init();
  
  Serial.println("Setup done");
}

void loop() {

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // loop VX messages
  vx.loop();

  unsigned long newUpdate = vx.getUpdated();
  if (lastUpdated != newUpdate) {
    // data hash changed
    if (publishData()) {
      lastUpdated = newUpdate;
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

boolean publishData() {
  boolean ret = true;

  StaticJsonBuffer<JSON_BUFFER_LENGTH> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  JsonObject& d = root.createNestedObject("d");

  d["T_IN"] = vx.getInsideTemp();
  d["T_OUT"] = vx.getOutsideTemp();
  d["T_INB"] = vx.getIncomingTemp();
  d["T_OUTB"] = vx.getExhaustTemp();
  d["ON"] = vx.isOn();
  d["RH_MODE"] = vx.isRhMode();
  d["RH"] = vx.getRh();
  d["HEATING_MODE"] = vx.isHeatingMode();
  d["SUMMER_MODE"] = vx.isSummerMode();
  d["HEATING"] = vx.isHeating();
  d["FAULT"] = vx.isFault();
  d["SERVICE"] = vx.isServiceNeeded();
  d["SPEED"] = vx.getFanSpeed();
  d["DEFAULT_SPEED"] = vx.getDefaultFanSpeed();
  d["SERVICE_PERIOD"] = vx.getServicePeriod();
  d["SERVICE_COUNTER"] = vx.getServiceCounter();
  d["HEATING_TARGET"] = vx.getHeatingTarget();

  if (DEBUG) {
    Serial.println("Publish payload:"); root.prettyPrintTo(Serial); Serial.println();
  }

  char buff[JSON_BUFFER_LENGTH];
  root.printTo(buff, JSON_BUFFER_LENGTH);

  if (client.publish(publishTopic, buff)) {
    if (DEBUG) {
      Serial.println("Publish OK");
    }
  } else {
    Serial.println("Publish FAILED");
    ret = false;
  }

  return ret;
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
  StaticJsonBuffer<JSON_BUFFER_LENGTH> jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject((char*)payload);
  if (!root.success()) {
    Serial.println("handleUpdate: payload parse FAILED");
    return;
  }

  if (DEBUG) {
    Serial.println("handleUpdate payload:"); root.prettyPrintTo(Serial); Serial.println();
  }

  JsonObject& d = root["d"];
  if (d.containsKey("ON")) {
    boolean isOn = d["ON"];
    if(isOn) {
      vx.setOn();
    } else {
      vx.setOff();
    }
  }
  if (d.containsKey("RH_MODE")) {
    boolean isOn = d["RH_MODE"];
    if(isOn) {
      vx.setRhModeOn();
    } else {
      vx.setRhModeOff();
    }
  }
  if (d.containsKey("HEATING_MODE")) {
    boolean isOn = d["HEATING_MODE"];
    if(isOn) {
      vx.setHeatingModeOn();
    } else {
      vx.setHeatingModeOff();
    }
  }
  if (d.containsKey("SPEED")) {
    int speed = d["SPEED"];
    vx.setFanSpeed(speed);
  }
  if (d.containsKey("HEATING_TARGET")) {
    int ht = d["HEATING_TARGET"];
    vx.setHeatingTarget(ht);
  }
  if (d.containsKey("SERVICE_PERIOD")) {
    int sp = d["SERVICE_PERIOD"];
    vx.setServicePeriod(sp);
  }
  if (d.containsKey("SERVICE_COUNTER")) {
    int sc = d["SERVICE_COUNTER"];
    vx.setServiceCounter(sc);
  }
}
