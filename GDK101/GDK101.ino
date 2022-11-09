#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <SoftwareSerial.h>
#include <LiquidCrystal_I2C.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "xCredentials.h"

#define OUTPUT_BUFFER_LENGTH 500
#define INPUT_BUFFER_LENGTH 300
#define M10_BUFFER_LEN 10

/* GDK101 commands */
enum {
 cmd_GAMMA_RESULT_QUERY = 0x44, // D, Read measuring value(10min avg, 1min update)
 cmd_GAMMA_RESULT_QUERY_1MIN = 0x4D, // M, Read measuring value(1min avg, 1min update)
 cmd_GAMMA_PROC_TIME_QUERY = 0x54, // T, Read measuring time
 cmd_GAMMA_MEAS_QUERY = 0x53, // S, Read status
 cmd_GAMMA_FW_VERSION_QUERY = 0x46, // F, Read firmware version
 cmd_GAMMA_VIB_STATUS_QUERY = 0x56, // V, Response vibration status
 cmd_GAMMA_RESET = 0x52, // R, Reset
 cmd_GAMMA_AUTO_SEND = 0x55, // U, Set Auto_send status
 cmd_GAMMA_ALL_QUERY = 0x41, // A, Read all data
};

const char publishTopic[] = "events/" DEVICE_ID; // publish events here
const char cmdTopic[] = "cmd/" DEVICE_ID;        // subscribe for commands here
const char AWS_endpoint[] = AWS_PREFIX ".iot.eu-west-1.amazonaws.com";
void callback(char* topic, byte* payload, unsigned int payloadLength);
char output_buffer[OUTPUT_BUFFER_LENGTH];
const char clientId[] = "ESP8266-" DEVICE_ID;

LiquidCrystal_I2C lcd(0x27, 16, 2);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");
WiFiClientSecure espClient;
PubSubClient client(AWS_endpoint, 8883, callback, espClient);
SoftwareSerial sws(12, 14);

char recData[50];
char tmpBuffer[10];
unsigned long last = 0L;
unsigned count = 0L;
char dots[] = {':', ' '};
bool isWifi = true;
float d10m[M10_BUFFER_LEN];
int d10m_index = 0;
bool isVal = false;

void setup() {
  Serial.begin(115200);
  sws.begin(9600);
  
  lcd.begin();
  lcd.backlight();
  lcd.clear();
  lcd.print("Starting up...");
  delay(2000);

  setup_wifi();
  delay(2000);

  setup_file_system();  
  delay(2000);
  
  lcd.clear();
  lcd.print("GDK101 FW ");
  lcd.print(getFwVersion());

  // Init buffer
  for(int i = 0;i < M10_BUFFER_LEN;i++) {
    d10m[i] = 0.0;
  }

  setAutosend();  
  delay(2000);

  Serial.println("Started...");
}

void loop() {
  if (isWifi) {
    if (!client.connected()) {
      reconnect();
    }
    client.loop();
  }
  
  float val = getValue();
  if (!isnan(val)) {
    Serial.print("d1m = ");
    Serial.print(val);
    Serial.println(" uSv/h");

    isVal = true;
    d10m[d10m_index++] = val;
    float d10m_avg = 0.0;
    for(int i = 0;i < M10_BUFFER_LEN;i++) {
      d10m_avg += d10m[i];
    }
    d10m_avg = d10m_avg / M10_BUFFER_LEN;

    lcd.clear();
    lcd.print(" 1m: ");
    lcd.print(val);
    lcd.print(" uSv/h");
    lcd.setCursor(0, 1);
    lcd.print("10m: ");
    lcd.print(d10m_avg);
    lcd.print(" uSv/h");

    if (d10m_index >= M10_BUFFER_LEN) {
      publishData(d10m_avg);
      d10m_index = 0;
    }
  }

  unsigned long now = millis();
  if (isVal && now - last > 2000) {
    last = now;
    count++;
    char dot = dots[count % 2];
    lcd.setCursor(3,0);
    lcd.print(dot);
    lcd.setCursor(3,1);
    lcd.print(dot);
  }
}

void setup_file_system() {
  lcd.clear();
  if (!SPIFFS.begin()) {
    lcd.print("Failed mount FS");
    return;
  }

  // Load certificate file
  File cert = SPIFFS.open("/cert.der", "r");
  if (!cert) {
    lcd.print("Failed open cert");
  }
  else
    lcd.print("OK open cert");

  lcd.setCursor(0,1);
  if (espClient.loadCertificate(cert))
    lcd.print("OK load cert");
  else
    lcd.print("Failed load cert");

  delay(1000);

  lcd.clear();
  // Load private key file
  File private_key = SPIFFS.open("/private.der", "r");
  if (!private_key) {
    lcd.print("Failed open pcert");
  }
  else
    lcd.print("OK open pcert");

  lcd.setCursor(0,1);
  if (espClient.loadPrivateKey(private_key))
    lcd.print("OK load pcert");
  else
    Serial.println("Failed load pcert");

  delay(1000);

  lcd.clear();
  // Load CA file
  File ca = SPIFFS.open("/ca.der", "r");
  if (!ca) {
    lcd.print("Failed open CA");
  }
  else
    lcd.print("OK open CA");

  lcd.setCursor(0,1);
  if (espClient.loadCACert(ca))
    lcd.print("OK load CA");
  else
    lcd.print("Failed load CA");
}

void setup_wifi() {
  // We start by connecting to a WiFi network
  espClient.setBufferSizes(512, 512);
  lcd.clear();
  lcd.print("Connecting WiFi");
  lcd.setCursor(0, 1);

  WiFi.begin(ssid, password);

  int count = 0;
  while (WiFi.status() != WL_CONNECTED) {
    if (count++ > 20) {
      isWifi = false;
      break;    
    }
    delay(1000);
    lcd.print(".");
  }

  if (isWifi) {
    WiFi.mode(WIFI_STA);
    lcd.clear();
    lcd.print("WiFi connected");
    lcd.setCursor(0, 1);
    lcd.print(WiFi.localIP());

    timeClient.begin();
    while (!timeClient.update()) {
      timeClient.forceUpdate();
    }

    espClient.setX509Time(timeClient.getEpochTime());
  }
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

  Serial.println("Publish payload:"); root.prettyPrintTo(Serial); Serial.println();

  root.printTo(output_buffer, OUTPUT_BUFFER_LENGTH);

  if (client.publish(publishTopic, output_buffer)) {
    Serial.println("Publish OK");
  } else {
    Serial.println("Publish FAILED");
    ret = false;
  }

  return ret;
}

boolean publishData(float d10m) {
  StaticJsonBuffer<OUTPUT_BUFFER_LENGTH> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  JsonObject& d = root.createNestedObject("d");

  d["D10M"] = d10m;

  return publishPayload(root);
}

void callback(char* topic, byte * payload, unsigned int length) {
  Serial.print("Callback invoked for topic: "); Serial.println(topic);

  if (strcmp (cmdTopic, topic) == 0) {
    handleUpdate(payload);
  }
}

void handleUpdate(byte * payload) {
  StaticJsonBuffer<INPUT_BUFFER_LENGTH> jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject((char*)payload);
  if (!root.success()) {
    Serial.println("handleUpdate: payload parse FAILED");
    return;
  }
  Serial.println("handleUpdate payload:"); root.prettyPrintTo(Serial); Serial.println();

  JsonObject& d = root["d"];

  if (d.containsKey("cmd")) {
    String cmd = d["cmd"];

    if (cmd == "reset") {
      reset();
    } 
  }
}

// GDK101 

/* Measurement Reset */
void reset() {
  byte send_data[6] = {0x02, cmd_GAMMA_RESET, ':', '1', 0x0D, 0x0A};
  sws.write(send_data, 6);
  delay(100);
}

/* Read Firmware */
const char* getFwVersion() {
  byte send_data[6] = {0x02, cmd_GAMMA_FW_VERSION_QUERY, ':', '?', 0x0D, 0x0A};
  sws.write(send_data, 6);
  delay(100);
  const char* res = recUartData('F');
  if (res != NULL) {
    substring(res, tmpBuffer, 1, 4);
    return tmpBuffer;
  } else {
    return NULL;
  }
}

float getValue() {
  const char* res = recUartData('M');
  if (res != NULL) {
    return atof(res);
  } else {
    return NAN;
  }
}

void setAutosend() {
  byte send_data[6] = {0x02, cmd_GAMMA_AUTO_SEND, ':', '1', 0x0D, 0x0A};
  sws.write(send_data, 6);
  delay(100);
}

const char* recUartData(char cmd) { 
  delay(100);
  while(sws.available()) {
    if(sws.read() == cmd) {
      sws.read(); // :
      int rec_size = sws.available();
      int i = 0;
      for (; i < rec_size; i++) {
        recData[i] = sws.read();
      }
      recData[i] = '\0';
//      Serial.println(recData);
        
      return recData;
    }
  }
    
  return NULL;
}

// C substring function definition
void substring(const char s[], char sub[], int p, int l) {
   int c = 0;
   
   while (c < l) {
      sub[c] = s[p+c-1];
      c++;
   }
   sub[c] = '\0';
}
