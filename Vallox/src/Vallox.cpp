// =======================================
// VALLOX DIGIT SE COMMUNICATION PROTOCOL
// =======================================

#include "Vallox.h"

// VX fan speed (1-8) conversion table
const int8_t vxFanSpeeds[] = {
  VX_FAN_SPEED_1, 
  VX_FAN_SPEED_2, 
  VX_FAN_SPEED_3, 
  VX_FAN_SPEED_4, 
  VX_FAN_SPEED_5, 
  VX_FAN_SPEED_6, 
  VX_FAN_SPEED_7, 
  VX_FAN_SPEED_8 
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

void Vallox::init() {
  if(isDebug) {
    Serial.println("Vallox init.");  
  }

  // set the data rate for the SoftwareSerial port
  serial->begin(9600);

  // init cache
  data.updated = millis();
  data.is_on = pollIsOn();
  data.is_rh_mode = pollIsRhMode();
  data.is_heating_mode = pollIsHeatingMode();
  data.is_summer_mode = pollIsSummerMode();
  data.is_filter = pollIsFilter();
  data.is_heating = pollIsHeating();
  data.is_fault = pollIsFault();
  data.is_service = pollIsService();
  data.fan_speed =  pollFanSpeed();
  data.default_fan_speed =  pollDefaultFanSpeed();
  data.rh = pollRh();
  data.service_period = pollServicePeriod();
  data.service_counter = pollServiceCounter();
  data.heating_target = pollHeatingTarget();
  data.t_outside = pollOutsideTemp();
  data.t_inside = pollInsideTemp();
  data.t_exhaust = pollExhaustTemp();
  data.t_incoming = pollIncomingTemp();
}

void Vallox::loop() {
  byte message[VX_MSG_LENGTH];

  // read and decode as long as messages are available
  while (readMessage(message)) {
    if (isDebug) {
      prettyPrint(message);
    }
    decodeMessage(message);
  }
}

// setters
// these will set data both in the bus and cache

void Vallox::setFanSpeed(int speed) {
  if (speed <= VX_MAX_FAN_SPEED) {
    setVariable(VX_VARIABLE_FAN_SPEED, fanSpeed2Hex(speed));
	data.fan_speed = speed;
  }
}

void Vallox::setDefaultFanSpeed(int speed) {
  if (speed < VX_MAX_FAN_SPEED) {
    setVariable(VX_VARIABLE_DEFAULT_FAN_SPEED, fanSpeed2Hex(speed));
	data.default_fan_speed = speed;
  }
}

void Vallox::setOn() {
  byte status = pollVariable(VX_VARIABLE_STATUS);
  setVariable(VX_VARIABLE_STATUS, status | VX_STATUS_FLAG_POWER);
  data.is_on = true;
}

void Vallox::setOff() {
  byte status = pollVariable(VX_VARIABLE_STATUS);
  setVariable(VX_VARIABLE_STATUS, status & ~VX_STATUS_FLAG_POWER);
  data.is_on = false;
}

void Vallox::setRhModeOn() {
  byte status = pollVariable(VX_VARIABLE_STATUS);
  setVariable(VX_VARIABLE_STATUS, status | VX_STATUS_FLAG_RH);
  data.is_rh_mode = true;
}

void Vallox::setRhModeOff() {
  byte status = pollVariable(VX_VARIABLE_STATUS);
  setVariable(VX_VARIABLE_STATUS, status & ~VX_STATUS_FLAG_RH);
  data.is_rh_mode = false;
}

void Vallox::setHeatingModeOn() {
  byte status = pollVariable(VX_VARIABLE_STATUS);
  setVariable(VX_VARIABLE_STATUS, status | VX_STATUS_FLAG_HEATING_MODE);
  data.is_heating_mode = true;
}

void Vallox::setHeatingModeOff() {
  byte status = pollVariable(VX_VARIABLE_STATUS);
  setVariable(VX_VARIABLE_STATUS, status & ~VX_STATUS_FLAG_HEATING_MODE);
  data.is_heating_mode = false;
}

void Vallox::setServicePeriod(int months) {
  if (months >= 0 && months < 256) {
    setVariable(VX_VARIABLE_SERVICE_PERIOD, months);
    data.service_period = months;
  }
}

void Vallox::setServiceCounter(int months) {
  if (months >= 0 && months < 256) {
    setVariable(VX_VARIABLE_SERVICE_COUNTER, months);
    data.service_counter = months;
  }
}

void Vallox::setHeatingTarget(int cel) {
  if (cel >= 10 && cel <= 27) {
    byte hex = htCel2Hex(cel);
    setVariable(VX_VARIABLE_HEATING_TARGET, hex);
    data.heating_target = cel;
  }
}

// getters get data from the cache

unsigned long Vallox::getUpdated() {
  return data.updated;
}

int Vallox::getInsideTemp() {
  return data.t_inside;
}

int Vallox::getOutsideTemp() {
  return data.t_outside;
}

int Vallox::getIncomingTemp() {
  return data.t_incoming;
}

int Vallox::getExhaustTemp() {
  return data.t_exhaust;
}

boolean Vallox::isOn() {
  return data.is_on;
}

boolean Vallox::isRhMode() {
  return data.is_rh_mode;
}

boolean Vallox::isHeatingMode() {
  return data.is_heating_mode;
}

boolean Vallox::isSummerMode() {
  return data.is_summer_mode;
}

boolean Vallox::isFilter() {
  return data.is_filter;
}

boolean Vallox::isHeating() {
  return data.is_heating;
}

boolean Vallox::isFault() {
  return data.is_fault;
}

boolean Vallox::isService() {
  return data.is_service;
}

int Vallox::getServicePeriod() {
  return data.service_period;
}

int Vallox::getServiceCounter() {
  return data.service_counter;
}

int Vallox::getFanSpeed() {
  return data.fan_speed;
}

int Vallox::getDefaultFanSpeed() {
  return data.default_fan_speed;
}

int Vallox::getRh() {
  return data.rh;
}

int Vallox::getHeatingTarget() {
  return data.heating_target;
}

// pollers poll data from the bus

int Vallox::pollInsideTemp() {
  byte ntc = pollVariable(VX_VARIABLE_T_INSIDE);
  return ntc2Cel(ntc);
}

int Vallox::pollOutsideTemp() {
  byte ntc = pollVariable(VX_VARIABLE_T_OUTSIDE);
  return ntc2Cel(ntc);
}

int Vallox::pollIncomingTemp() {
  byte ntc = pollVariable(VX_VARIABLE_T_INCOMING);
  return ntc2Cel(ntc);
}

int Vallox::pollExhaustTemp() {
  byte ntc = pollVariable(VX_VARIABLE_T_EXHAUST);
  return ntc2Cel(ntc);
}

boolean Vallox::pollIsOn() {
  byte status = pollVariable(VX_VARIABLE_STATUS);
  return (status & VX_STATUS_FLAG_POWER) != 0x00;
}

boolean Vallox::pollIsRhMode() {
  byte status = pollVariable(VX_VARIABLE_STATUS);
  return (status & VX_STATUS_FLAG_RH) != 0x00;
}

boolean Vallox::pollIsHeatingMode() {
  byte status = pollVariable(VX_VARIABLE_STATUS);
  return (status & VX_STATUS_FLAG_HEATING_MODE) != 0x00;
}

boolean Vallox::pollIsSummerMode() {
  byte io = pollVariable(VX_VARIABLE_IO_08);
  return (io & 0x02) != 0x00;
}

boolean Vallox::pollIsFilter() {
  byte status = pollVariable(VX_VARIABLE_STATUS);
  return (status & VX_STATUS_FLAG_FILTER) != 0x00;
}

boolean Vallox::pollIsHeating() {
  byte status = pollVariable(VX_VARIABLE_STATUS);
  return (status & VX_STATUS_FLAG_HEATING) != 0x00;
}

boolean Vallox::pollIsFault() {
  byte status = pollVariable(VX_VARIABLE_STATUS);
  return (status & VX_STATUS_FLAG_FAULT) != 0x00;
}

boolean Vallox::pollIsService() {
  byte status = pollVariable(VX_VARIABLE_STATUS);
  return (status & VX_STATUS_FLAG_SERVICE) != 0x00;
}

int Vallox::pollServicePeriod() {
  return pollVariable(VX_VARIABLE_SERVICE_PERIOD);
}

int Vallox::pollServiceCounter() {
  return pollVariable(VX_VARIABLE_SERVICE_COUNTER);
}

int Vallox::pollFanSpeed() {
  byte speed = pollVariable(VX_VARIABLE_FAN_SPEED);
  return hex2FanSpeed(speed);
}

int Vallox::pollDefaultFanSpeed() {
  byte speed = pollVariable(VX_VARIABLE_DEFAULT_FAN_SPEED);
  return hex2FanSpeed(speed);
}

int Vallox::pollRh() {
  byte hex = pollVariable(VX_VARIABLE_RH);
  int rh = hex2Rh(rh);

  if(rh >= 0 && rh <= 100) {
    return rh;
  } else {
    return NOT_SET; 
  }
}

int Vallox::pollHeatingTarget() {
  byte hex = pollVariable(VX_VARIABLE_HEATING_TARGET);
  return hex2HtCel(hex);
}

// private

// set generic variable value in all mainboards and panels
void Vallox::setVariable(byte variable, byte value) {
  byte message[VX_MSG_LENGTH];

  message[0] = VX_MSG_DOMAIN;
  message[1] = VX_MSG_PANEL_1;
  message[2] = VX_MSG_MAINBOARDS;
  message[3] = variable;
  message[4] = value;
  message[5] = calculateCheckSum(message);

  // send to all mainboards
  for (int i = 0; i < VX_MSG_LENGTH; i++) {
    serial->write(message[i]);
  }
  
  message[1] = VX_MSG_MAINBOARD_1;
  message[2] = VX_MSG_PANELS;
  message[5] = calculateCheckSum(message);

  // send to all panels
  for (int i = 0; i < VX_MSG_LENGTH; i++) {
    serial->write(message[i]);
  }

  if(isDebug) {
    Serial.print("Variable ");Serial.print(variable, HEX);
	Serial.print(" set to ");Serial.println(value, HEX);
  }
}

// poll variable value from mainboard 1
byte Vallox::pollVariable(byte variable) {
  if(isDebug) {
    Serial.print("Polled variable ");Serial.print(variable, HEX);Serial.print(" = ");
  }

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

  // leave time for the reply to arrive
  delay(10);
  
  byte reply[VX_MSG_LENGTH];
  while (readMessage(reply)) {
    if (reply[2] == VX_MSG_PANEL_1 && reply[3] == variable) {
      ret = reply[4];
      break;
    }
  }

  if(isDebug) {
    Serial.println(ret, HEX);
  }

  return ret;
}

// tries to read one full message
// returns true if a message was read, false otherwise
boolean Vallox::readMessage(byte message[]) {
  boolean ret = false;

  if (serial->available() >= VX_MSG_LENGTH) {
    message[0] = serial->read();

    if (message[0] == VX_MSG_DOMAIN) {
      message[1] = serial->read();
      message[2] = serial->read();

      // accept messages from mainboard 1 or panel 1
      // accept messages to panel 1, mainboard 1 or to all panels and mainboards
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

void Vallox::decodeMessage(const byte message[]) {
  // decode variable in message
  byte variable = message[3];
  byte value = message[4];
  unsigned long now = millis();

  if (variable == VX_VARIABLE_T_OUTSIDE) {
    int cel = ntc2Cel(value);
    if(cel != data.t_outside) {
      data.t_outside = cel;
      data.updated = now;
    }
  } else if (variable == VX_VARIABLE_T_EXHAUST) {
    int cel = ntc2Cel(value);
    if(cel != data.t_exhaust) {
      data.t_exhaust = cel;
      data.updated = now;
    }
  } else if (variable == VX_VARIABLE_T_INSIDE) {
    int cel = ntc2Cel(value);
    if(cel != data.t_inside) {
      data.t_inside = cel;
      data.updated = now;
    }
  } else if (variable == VX_VARIABLE_T_INCOMING) {
    int cel = ntc2Cel(value);
    if(cel != data.t_incoming) {
      data.t_incoming = cel;
      data.updated = now;
    }
  } else if (variable == VX_VARIABLE_FAN_SPEED) {
    int speed = hex2FanSpeed(value);
    if(speed != data.fan_speed) {
      data.fan_speed = speed;
      data.updated = now;
    }
  } else if (variable == VX_VARIABLE_DEFAULT_FAN_SPEED) {
    int speed = hex2FanSpeed(value);
    if(speed != data.default_fan_speed) {
      data.default_fan_speed = speed;
      data.updated = now;
    }
  } else if (variable == VX_VARIABLE_STATUS) {
    decodeStatus(value);
  } else if (variable == VX_VARIABLE_IO_08) {
    boolean summer_mode = (value & 0x01) != 0x00;
    if(summer_mode != data.is_summer_mode) {
      data.is_summer_mode = summer_mode;
      data.updated = now;
    }
  } else if (variable == VX_VARIABLE_SERVICE_PERIOD) {
    if(value != data.service_period) {
      data.service_period = value;
      data.updated = now;
    }
  } else if (variable == VX_VARIABLE_SERVICE_COUNTER) {
    if(value != data.service_counter) {
      data.service_counter = value;
      data.updated = now;
    }
  } else if (variable == VX_VARIABLE_RH) {
    int rh = hex2Rh(value);
    if(rh != data.rh) {
      data.rh = rh;
      data.updated = now;
    }
  } else if (variable == VX_VARIABLE_HEATING_TARGET) {
    int cel = hex2HtCel(value);
    if(cel != data.heating_target) {
      data.heating_target = cel;
      data.updated = now;
    }
  } else {
    // variable not recognized
  }
}

void Vallox::decodeStatus(byte status) {
  unsigned long now = millis();
  
  boolean on = (status & VX_STATUS_FLAG_POWER) != 0x00;
  if(on != data.is_on) {
    data.is_on = on;
    data.updated = now;
  }

  boolean rh_mode = (status & VX_STATUS_FLAG_RH) != 0x00;
  if(rh_mode != data.is_rh_mode) {
    data.is_rh_mode = rh_mode;
    data.updated = now;
  }


  boolean heating_mode = (status & VX_STATUS_FLAG_HEATING_MODE) != 0x00;
  if(heating_mode != data.is_heating_mode){
    data.is_heating_mode = heating_mode;
    data.updated = now;
  }
  
  boolean filter = (status & VX_STATUS_FLAG_FILTER) != 0x00;
  if(filter != data.is_filter) {
    data.is_filter = filter;
    data.updated = now;
  }
  
  boolean heating = (status & VX_STATUS_FLAG_HEATING) != 0x00;
  if(heating != data.is_heating) {
    data.is_heating = heating;
    data.updated = now;
  }
  
  boolean fault = (status & VX_STATUS_FLAG_FAULT) != 0x00;
  if(fault != data.is_fault) {
    data.is_fault = fault;
    data.updated = now;
  }
  
  boolean service = (status & VX_STATUS_FLAG_SERVICE) != 0x00;
  if(service != data.is_service) {
    data.is_service = service;
    data.updated = now;
  }
}

int Vallox::ntc2Cel(byte ntc) {
  int i = (int)ntc;
  return vxTemps[i];
}

byte Vallox::cel2Ntc(int cel) {
  for (int i = 0; i < 256; i++) {
    if(vxTemps[i] == cel) {
      return i;
    }
  }

  // we should not be here, return 10 Cel as default
  return 0x83;
}

byte Vallox::fanSpeed2Hex(int fan) {
  if (fan > 0 && fan < 9) {
    return vxFanSpeeds[fan - 1];
  }

  // we should not be here, return speed 1 as default
  return VX_FAN_SPEED_1;
}

int Vallox::hex2FanSpeed(byte hex) {
  for (int i = 0; i < sizeof(vxFanSpeeds); i++) {
    if (vxFanSpeeds[i] == hex) {
      return i + 1;
    }
  }

  return NOT_SET;
}

int Vallox::hex2Rh(byte hex) {
  if (hex >= 0x33) {
    return (hex - 51) / 2.04;;
  } else {
    return NOT_SET;
  }
}

int Vallox::hex2HtCel(byte hex) {
  if (hex == 0x01) {
    return 10;
  } else if (hex == 0x03) {
    return 13;
  } else if (hex == 0x07) {
    return 15;
  } else if (hex == 0x0F) {
    return 18;
  } else if (hex == 0x1F) {
    return 20;
  } else if (hex == 0x3F) {
    return 23;
  } else if (hex == 0x7F) {
    return 25;
  } else if (hex == 0xFF) {
    return 27;
  } else {
	return NOT_SET;
  }
}

byte Vallox::htCel2Hex(int htCel) {
  if (htCel < 13) {
    return 0x01;
  } else if (htCel < 15) {
    return 0x03;	  
  } else if (htCel < 18) {
    return 0x07;	  
  } else if (htCel < 20) {
    return 0x0F;	  
  } else if (htCel < 23) {
    return 0x1F;	  
  } else if (htCel < 25) {
    return 0x3F;	  
  } else if (htCel < 27) {
    return 0x7F;	  
  } else if (htCel == 27) {
    return 0xFF;
  } else {
    return 0x01;
  }	  
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
  for (int i = 0; i < VX_MSG_LENGTH; i++) {
    Serial.print(message[i], HEX); Serial.print(" ");
  }
  Serial.println();
}

boolean vxIsSet(int value) {
  return value != NOT_SET;
}

