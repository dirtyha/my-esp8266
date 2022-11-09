#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <SoftwareSerial.h>
#include <ArduinoJson.h>
#include <IHC.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "IHCConfig.h"
#include "xCredentials.h"

#define OUTPUT_BUFFER_LENGTH 500
#define INPUT_BUFFER_LENGTH 300
#define DEBUG 1
#define DEBUG_IHC 0

const char publishTopic[] = "events/" DEVICE_ID; // publish events here
const char cmdTopic[] = "cmd/" DEVICE_ID;        // subscribe for commands here
const char AWS_endpoint[] = AWS_PREFIX ".iot.eu-west-1.amazonaws.com";
void callback(char* topic, byte* payload, unsigned int payloadLength);
char output_buffer[OUTPUT_BUFFER_LENGTH];
const char clientId[] = "ESP8266-" DEVICE_ID;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");
WiFiClientSecure espClient;
PubSubClient client(AWS_endpoint, 8883, callback, espClient);
SoftwareSerial sws(4, 5);
IHC ihc;
IHCRS485Packet sendPacket;
unsigned long pulse_duration = 0;
unsigned short pulse_module = 0;
unsigned short pulse_port = 0;
unsigned long pulse_on = 0;
unsigned long pulse_off = 0;
unsigned short state = 0;
unsigned long lastHeartBeat = 0;
int status = 0;

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

  ihc.init(&sws, DEBUG_IHC);

  if (DEBUG) {
    Serial.println("Setup done");
  }
}

void loop() {

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  ihc.loop();

  unsigned long now = millis();
  if (state == 1) {
    if (now - pulse_on > 100) {
      pulseOff();
    }
  }

  if (state == 3) {
    if (now - pulse_on > pulse_duration) {
      pulseOff();
    }
  }

  if (state == 2) {
    if (now - pulse_off > 500) {
      pulseOn();
    }
  }

  status = ihc.getStatus();
  if (now - lastHeartBeat > 10000) {
    lastHeartBeat = now;
    publishStatus();
  }

  IHCRS485Packet *packet = ihc.receive();
  if (packet != NULL) {
    switch (packet->getDataType()) {
      case IHCDefs::OUTP_STATE:
        {
          unsigned long changeId = updateStates(packet->getData());
          if (changeId > 0) {
            for (int i = 0; i < SIZE; i++) {
              IHCIO io = ios[i];
              if (io.getChangeId() == changeId) {
                publishData(io);
              }
            }
          }
        }
        break;

      default:
        ;
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
      client.subscribe(cmdTopic, 1);
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

boolean publishData(IHCIO & io) {
  StaticJsonBuffer<OUTPUT_BUFFER_LENGTH> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  JsonObject& d = root.createNestedObject("d");

  d["module"] = io.getModule();
  d["port"] = io.getPort();
  d["name"] = io.getName();
  d["type"] = io.isOutput() ? "output" : "input";
  d["state"] = io.getState();

  return publishPayload(root);
}

boolean publishStatus() {
  boolean ret = true;

  StaticJsonBuffer<OUTPUT_BUFFER_LENGTH> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  JsonObject& d = root.createNestedObject("d");

  d["status"] = status;

  return publishPayload(root);
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

    if (cmd == "status") {
      // Publish status
      publishStatus();
    } else if (cmd == "data") {
      // Publish data
      for (int i = 0; i < SIZE; i++)
      {
        publishData(ios[i]);
      }
    } else if (cmd == "change") {
      unsigned short module = d["module"];
      unsigned short port = d["port"];
      bool state = d["state"];
      Vector<byte> *pData = changeOutput(module, port, state);
      sendPacket.setData(IHCDefs::ID_IHC, IHCDefs::SET_OUTPUT, pData);
      ihc.send(&sendPacket);
    } else if (cmd == "pulse") {
      pulse_duration = d["duration"];
      pulse_module = d["module"];
      pulse_port = d["port"];

      pulseOn();
    }
  }
}

void pulseOn() {
  if (state == 0) {
    state = 1;
  }

  if (state == 2) {
    state = 3;
  }
  
  Vector<byte> *pData = changeOutput(pulse_module, pulse_port, true);
  sendPacket.setData(IHCDefs::ID_IHC, IHCDefs::SET_OUTPUT, pData);
  ihc.send(&sendPacket);
  pulse_on = millis();
  Serial.print("Pulse ON, state=");Serial.println(state);
}

void pulseOff() {
  if (state == 3) {
    state = 0;
  }

  if (state == 1) {
    if (pulse_duration > 0) {
      state = 2;  
    } else {
      state = 0;
    }
  }

  Vector<byte> *pData = changeOutput(pulse_module, pulse_port, false);
  sendPacket.setData(IHCDefs::ID_IHC, IHCDefs::SET_OUTPUT, pData);
  ihc.send(&sendPacket);
  pulse_off = millis();
  Serial.print("Pulse OFF, state=");Serial.println(state);
}
