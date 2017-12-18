#include <Arduino.h>
#include <SoftwareSerial.h>
#include "xVallox.h"

// VX fan speed (1-8) conversion table
const int8_t vxFanSpeeds[] = {
  0x01, 0x03, 0x07, 0x0f, 0x1f, 0x3f, 0x7f, 0xff
};

// VX NTC temperature conversion table
const int8_t vxTemps[] = {
  -74, -70, -66, -62, -59, -56, -54, -52, -50, -48, // 0x00 - 0x09
  -47, -46, -44, -43, -42, -41, -40, -39, -38, -37, // 0x0a - 0x13
  -36, -35, -34, -33, -33, -32, -31, -30, -30, -29, // 0x14 - 0x1d
  -28, -28, -27, -27, -26, -25, -25, -24, -24, -23, // 0x1e - 0x27
  -23, -22, -22, -21, -21, -20, -20, -19, -19, -19, // 0x28 - 0x31
  -18, -18, -17, -17, -16, -16, -16, -15, -15, -14, // 0x32 - 0x3b
  -14, -14, -13, -13, -12, -12, -12, -11, -11, -11, // 0x3c - 0x45
  -10, -10, -9, -9, -9, -8, -8, -8, -7, -7,         // 0x46 - 0x4f
  -7, -6, -6, -6, -5, -5, -5, -4, -4, -4,           // 0x50 - 0x59
  -3, -3, -3, -2, -2, -2, -1, -1, -1, -1,           // 0x5a - 0x63
  0,  0,  0,  1,  1,  1,  2,  2,  2,  3,            // 0x64 - 0x6d
  3,  3,  4,  4,  4,  5,  5,  5,  5,  6,            // 0x6e - 0x77
  6,  6,  7,  7,  7,  8,  8,  8,  9,  9,            // 0x78 - 0x81
  9, 10, 10, 10, 11, 11, 11, 12, 12, 12,            // 0x82 - 0x8b
  13, 13, 13, 14, 14, 14, 15, 15, 15, 16,           // 0x8c - 0x95
  16, 16, 17, 17, 18, 18, 18, 19, 19, 19,           // 0x96 - 0x9f
  20, 20, 21, 21, 21, 22, 22, 22, 23, 23,           // 0xa0 - 0xa9
  24, 24, 24, 25, 25, 26, 26, 27, 27, 27,           // 0xaa - 0xb3
  28, 28, 29, 29, 30, 30, 31, 31, 32, 32,           // 0xb4 - 0xbd
  33, 33, 34, 34, 35, 35, 36, 36, 37, 37,           // 0xbe - 0xc7
  38, 38, 39, 40, 40, 41, 41, 42, 43, 43,           // 0xc8 - 0xd1
  44, 45, 45, 46, 47, 48, 48, 49, 50, 51,           // 0xd2 - 0xdb
  52, 53, 53, 54, 55, 56, 57, 59, 60, 61,           // 0xdc - 0xe5
  62, 63, 65, 66, 68, 69, 71, 73, 75, 77,           // 0xe6 - 0xef
  79, 81, 82, 86, 90, 93, 97, 100, 100, 100,        // 0xf0 - 0xf9
  100, 100, 100, 100, 100, 100                      // 0xfa - 0xff
};

// public

Vallox::Vallox(byte rx, byte tx) {
  Vallox(rx, tx, false);
}

Vallox::Vallox(byte rx, byte tx, boolean debug) {
  serial = new SoftwareSerial(rx, tx);
  isDebug = debug;  
}

vx_data* Vallox::init() {
  // set the data rate for the SoftwareSerial port
  serial->begin(9600);

  // init data
  data.t_outside =  NOT_SET;  // read only
  data.t_inside =   NOT_SET;  // read only
  data.t_outbound = NOT_SET;  // read only
  data.t_inbound =  NOT_SET;  // read only
  data.fan_speed =  getFanSpeed();
  data.power = getPower();

  return &data;
}

vx_data* Vallox::loop() {
  byte message[VX_MSG_LENGTH];

  // read and decode all available messages
  while (readMessage(message)) {
    decodeMessage(message);
    if(isDebug) {
      prettyPrint(message);
    }
  }

  return &data;
}

// setters
void Vallox::setFanSpeed(int speed) {
  setVariable(VX_VARIABLE_FAN_SPEED, fanSpeed2Hex(speed));
}

void Vallox::setOn() {
  byte status = getVariable(VX_VARIABLE_STATUS);
  setVariable(VX_VARIABLE_STATUS, status | VX_STATUS_FLAG_POWER);
}

void Vallox::setOff() {
  byte status = getVariable(VX_VARIABLE_STATUS);
  setVariable(VX_VARIABLE_STATUS, status & ~VX_STATUS_FLAG_POWER);
}

// getters
int Vallox::getFanSpeed() {
  byte speed = getVariable(VX_VARIABLE_FAN_SPEED);
  return hex2FanSpeed(speed);
}

boolean Vallox::getPower() {
  byte status = getVariable(VX_VARIABLE_STATUS);
  return status | VX_STATUS_FLAG_POWER;
}

// set variable value in mainboards and panels
void Vallox::setVariable(byte variable, byte value) {
  byte message[VX_MSG_LENGTH];

  message[0] = VX_MSG_DOMAIN;
  message[1] = VX_MSG_PANEL_1;
  message[2] = VX_MSG_MAINBOARDS;
  message[3] = variable;
  message[4] = value;
  message[5] = calculateCheckSum(message);

  // send to mainboards
  for (int i = 0; i < VX_MSG_LENGTH; i++) {
    serial->write(message[i]);
  }

  message[1] = VX_MSG_MAINBOARD_1;
  message[2] = VX_MSG_PANELS;
  message[5] = calculateCheckSum(message);

  // send to panels
  for (int i = 0; i < VX_MSG_LENGTH; i++) {
    serial->write(message[i]);
  }
}

// get variable value from mainboards
byte Vallox::getVariable(byte variable) {
  byte ret = 0x00;

  byte message[VX_MSG_LENGTH];
  message[0] = VX_MSG_DOMAIN;
  message[1] = VX_MSG_PANEL_1;
  message[2] = VX_MSG_MAINBOARD_1;
  message[3] = VX_MSG_POLL_BYTE;
  message[4] = variable;
  message[5] = calculateCheckSum(message);

  for (int i = 0; i < VX_MSG_LENGTH; i++) {
    serial->write(message[i]);
  }

  delay(10);

  byte reply[VX_MSG_LENGTH];
  while (readMessage(reply)) {
    if (reply[2] == VX_MSG_PANEL_1 && reply[3] == variable) {
      ret = reply[4];
      break;
    }
  }

  return ret;
}

// private

// reads one full message
boolean Vallox::readMessage(byte message[]) {
  boolean ret = false; // true = new message has been received, false = no message

  if (serial->available() >= VX_MSG_LENGTH) {
    message[0] = serial->read();

    if (message[0] == VX_MSG_DOMAIN) {
      message[1] = serial->read();
      message[2] = serial->read();

      // accept messages from mainboard to panel 1 or all panels
      // accept messages from panel to mainboard 1 or all mainboards
      if ((message[1] == VX_MSG_MAINBOARD_1 || message[1] == VX_MSG_PANEL_1) &&
          (message[2] == VX_MSG_PANELS || message[2] == VX_MSG_PANEL_1 ||
           message[2] == VX_MSG_MAINBOARD_1 || message[2] == VX_MSG_MAINBOARDS)) {
        int i = 3;
        // read the rest of the message
        while (i < VX_MSG_LENGTH) {
          message[i++] = serial->read();
        }

        ret = true;
      }
    }
  }

  return ret;
}

void Vallox::decodeMessage(byte message[]) {
  // decode variable in message
  byte variable = message[3];
  byte value = message[4];

  if (variable == VX_VARIABLE_T_OUTSIDE) {
    data.t_outside = ntc2Cel(value);
  } else if (variable == VX_VARIABLE_T_OUTBOUND) {
    data.t_outbound = ntc2Cel(value);
  } else if (variable == VX_VARIABLE_T_INSIDE) {
    data.t_inside = ntc2Cel(value);
  } else if (variable == VX_VARIABLE_T_INBOUND) {
    data.t_inbound = ntc2Cel(value);
  } else if (variable == VX_VARIABLE_FAN_SPEED) {
    data.fan_speed = hex2FanSpeed(value);
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

int Vallox::ntc2Cel(byte b) {
  int i = (int)b;
  return vxTemps[i];
}

byte Vallox::fanSpeed2Hex(int val) {
  return vxFanSpeeds[val - 1];
}

int Vallox::hex2FanSpeed(byte b) {
  for (int i = 0; i < sizeof(vxFanSpeeds); i++) {
    if (vxFanSpeeds[i] == b) {
      return i + 1;
    }
  }

  return 0;
}

// calculate VX message checksum
byte Vallox::calculateCheckSum(const byte message[]) {
  byte ret = 0x00;
  for (int i = 0; i < VX_MSG_LENGTH - 1; i++) {
    ret += message[i];
  }

  return ret;
}

void Vallox::prettyPrint(const byte message[]) {
  Serial.print("MSG: ");
  for(int i = 0;i < VX_MSG_LENGTH;i++) {
    Serial.print(message[i], HEX);Serial.print(" ");
  }
  Serial.println();
}

boolean vxIsSet(int value) {
  return value != NOT_SET;
}

