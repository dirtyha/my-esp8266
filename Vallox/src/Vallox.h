// =======================================
// VALLOX DIGIT SE COMMUNICATION PROTOCOL
// =======================================

#ifndef VALLOX_H
#define VALLOX_H

#include <Arduino.h>
#include <SoftwareSerial.h>

#define VX_MSG_LENGTH 6
#define VX_MSG_DOMAIN 0x01
#define VX_MSG_POLL_BYTE 0x00
#define NOT_SET -999;

// senders and receivers
#define VX_MSG_MAINBOARD_1 0x11
#define VX_MSG_MAINBOARDS 0x10
#define VX_MSG_PANEL_1 0x21
#define VX_MSG_PANELS 0x20

// variables
#define VX_VARIABLE_STATUS 0xA3
#define VX_VARIABLE_FAN_SPEED 0x29
#define VX_VARIABLE_DEFAULT_FAN_SPEED 0xA9
#define VX_VARIABLE_RH 0x2F
#define VX_VARIABLE_SERVICE_PERIOD 0xA6
#define VX_VARIABLE_SERVICE_COUNTER 0xAB
#define VX_VARIABLE_T_OUTSIDE 0x58
#define VX_VARIABLE_T_INSIDE 0x5a
#define VX_VARIABLE_T_EXHAUST 0x5C
#define VX_VARIABLE_T_INCOMING 0x5B
#define VX_VARIABLE_IO_08 0x08
#define VX_VARIABLE_HEATING_TARGET 0xA4

// status flags of variable A3
#define VX_STATUS_FLAG_POWER 0x01           // bit 0 read/write
#define VX_STATUS_FLAG_CO2 0x02             // bit 1 read/write
#define VX_STATUS_FLAG_RH 0x04              // bit 2 read/write
#define VX_STATUS_FLAG_HEATING_MODE 0x08    // bit 3 read/write
#define VX_STATUS_FLAG_FILTER 0x10          // bit 4 read
#define VX_STATUS_FLAG_HEATING 0x20         // bit 5 read
#define VX_STATUS_FLAG_FAULT 0x40           // bit 6 read
#define VX_STATUS_FLAG_SERVICE 0x80         // bit 7 read             

// fan speeds
#define VX_FAN_SPEED_1 0x01
#define VX_FAN_SPEED_2 0x03
#define VX_FAN_SPEED_3 0x07
#define VX_FAN_SPEED_4 0x0F
#define VX_FAN_SPEED_5 0x1F
#define VX_FAN_SPEED_6 0x3F
#define VX_FAN_SPEED_7 0x7F
#define VX_FAN_SPEED_8 0xFF
#define VX_MIN_FAN_SPEED 1
#define VX_MAX_FAN_SPEED 8

class Vallox {
  public:
    Vallox(byte rx, byte tx);
    Vallox(byte rx, byte tx, boolean isDebug);

    // get data from memory
    unsigned long getUpdated(); // time when data was updated
    int getInsideTemp();
    int getOutsideTemp();
    int getIncomingTemp();
    int getExhaustTemp();
    boolean isOn();
    boolean isRhMode();
    boolean isHeatingMode();
    boolean isSummerMode();
    boolean isFilter();
    boolean isHeating();
    boolean isFault();
    boolean isService();
    int getFanSpeed();
    int getDefaultFanSpeed();
    int getRh();
    int getServicePeriod();
    int getServiceCounter();
    int getHeatingTarget();
    byte getVariable(byte variable);

    // set data in Vallox bus
    void setFanSpeed(int speed);
    void setDefaultFanSpeed(int speed);
    void setOn();
    void setOff();
    void setRhModeOn();
    void setRhModeOff();
    void setHeatingModeOn();
    void setHeatingModeOff();
    void setServicePeriod(int months);
    void setServiceCounter(int months);
    void setHeatingTarget(int temp);
    void setVariable(byte variable, byte value);

    // initialize data - this polls and inits all data
    void init();
    // listen bus for data that arrives without polling
    void loop();

  private:
    SoftwareSerial* serial;
    boolean isDebug = false;
    
    // measured data from ValloxDSE
    struct {
      unsigned long updated;

      boolean is_on;
      boolean is_rh_mode;
      boolean is_heating_mode;
      boolean is_summer_mode;
      boolean is_filter;
      boolean is_heating;
      boolean is_fault;
      boolean is_service;

      int t_outside;
      int t_inside;
      int t_exhaust;
      int t_incoming;

      int fan_speed;
      int default_fan_speed;
      int rh;
      int service_period;
      int service_counter;
      int heating_target;
    } data;

    // pollers
    boolean pollIsOn();
    boolean pollIsRhMode();
    boolean pollIsHeatingMode();
    boolean pollIsSummerMode();
    boolean pollIsFilter();
    boolean pollIsHeating();
    boolean pollIsFault();
    boolean pollIsService();
	int pollInsideTemp();
	int pollOutsideTemp();
	int pollIncomingTemp();
	int pollExhaustTemp();
    int pollFanSpeed();
    int pollDefaultFanSpeed();
    int pollRh();
    int pollServicePeriod();
    int pollServiceCounter();
    int pollHeatingTarget();
    byte pollVariable(byte variable);

    // conversions
    static byte fanSpeed2Hex(int fan);
    static int hex2FanSpeed(byte hex);
    static int ntc2Cel(byte ntc);
    static byte cel2Ntc(int cel);
    static int hex2Rh(byte hex);

    // read and decode messages
    boolean readMessage(byte message[]);
    void decodeMessage(const byte message[]);
    void decodeStatus(byte status);

    // helpers
    static byte calculateCheckSum(const byte message[]);
    void prettyPrint(const byte message[]);
};

// helpers
boolean vxIsSet(int value);

#endif

