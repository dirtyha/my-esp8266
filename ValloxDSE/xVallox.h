// =======================================
// VALLOX DIGIT SE COMMUNICATION PROTOCOL
// =======================================

#include <Arduino.h>

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
#define VX_VARIABLE_T_OUTBOUND 0x5C
#define VX_VARIABLE_T_INBOUND 0x5B
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

// measured data from ValloxDSE
typedef struct {
  boolean is_on;
  boolean is_CO2_mode;
  boolean is_RH_mode;
  boolean is_heating_mode;
  boolean is_summer_mode;

  int t_outside;
  int t_inside;
  int t_outbound;
  int t_inbound;

  int fan_speed;
  int default_fan_speed;
} vx_data;

class Vallox {
  public:
    Vallox(byte rx, byte tx);
    Vallox(byte rx, byte tx, boolean isDebug);

    // getters
    int getFanSpeed();
    int getDefaultFanSpeed();
    boolean isOn();
    boolean isCO2Mode();
    boolean isRHMode();
    boolean isHeatingMode();
    boolean isSummerMode();
    boolean isFilter();
    boolean isHeating();
    boolean isFault();
    boolean isService();
    int getServicePeriod();
    int getServiceCounter();
    int getHeatingTarget();
    byte getVariable(byte variable);

    // setters
    void setFanSpeed(int speed);
    void setDefaultFanSpeed(int speed);
    void setOn();
    void setOff();
    void setCO2Mode();
    void setRHMode();
    void setHeatingMode();
    void setServicePeriod(int months);
    void setServiceCounter(int months);
    void setHeatingTarget(int temp);
    void setVariable(byte variable, byte value);

    // read and decode messages
    vx_data* init();
    vx_data* loop();

  private:
    SoftwareSerial* serial;
    boolean isDebug = false;
    vx_data data;

    // conversions
    static byte fanSpeed2Hex(int fan);
    static int hex2FanSpeed(byte b);
    static int ntc2Cel(byte b);
    static byte cel2Ntc(int cel);

    // read and decode messages
    boolean readMessage(byte message[]);
    void decodeMessage(const byte message[]);

    // helpers
    static byte calculateCheckSum(const byte message[]);
    void prettyPrint(const byte message[]);    
};

// helpers
boolean vxIsSet(int value);


