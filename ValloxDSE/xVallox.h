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
#define VX_VARIABLE_RH 0x2F
#define VX_VARIABLE_SERVICE_PERIOD 0xA6
#define VX_VARIABLE_SERVICE_COUNTER 0xAB
#define VX_VARIABLE_T_OUTSIDE 0x58
#define VX_VARIABLE_T_INSIDE 0x5a
#define VX_VARIABLE_T_OUTBOUND 0x5c
#define VX_VARIABLE_T_INBOUND 0x5b

// status flags
#define VX_STATUS_FLAG_POWER 0x01           // bit 0 read/write
#define VX_STATUS_FLAG_CO2 0x02             // bit 1 read/write
#define VX_STATUS_FLAG_RH 0x04              // bit 2 read/write
#define VX_STATUS_FLAG_HEATING_SWITCH 0x08  // bit 3 read/write
#define VX_STATUS_FILTER 0x10               // bit 4 read
#define VX_STATUS_HEATING_ON 0x20           // bit 5 read
#define VX_STATUS_FAULT 0x40                // bit 6 read
#define VX_STATUS_SERVICE 0x80              // bit 7 read             

// measured data from ValloxDSE
typedef struct {
  int t_outside;
  int t_inside;
  int t_outbound;
  int t_inbound;
  int fan_speed;
  boolean power;
} vx_data;

class Vallox {
  public:
    Vallox(byte rx, byte tx);
    Vallox(byte rx, byte tx, boolean isDebug);

    // getters
    int getFanSpeed();
    boolean getPower();
    byte getVariable(byte variable);

    // setters
    void setFanSpeed(int speed);
    void setOn();
    void setOff();
    void setVariable(byte variable, byte value);

    // read and decode messages
    vx_data* init();
    vx_data* loop();

  private:
    SoftwareSerial* serial;
    boolean isDebug = false;
    vx_data data;

    // conversions
    static byte fanSpeed2Hex(int val);
    static int hex2FanSpeed(byte b);
    static int ntc2Cel(byte b);

    // read and decode messages
    boolean readMessage(byte message[]);
    void decodeMessage(const byte message[]);

    // helpers
    static byte calculateCheckSum(const byte message[]);
    void prettyPrint(const byte message[]);    
};

// helpers
boolean vxIsSet(int value);


