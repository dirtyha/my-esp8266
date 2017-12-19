#include <Vallox.h>

Vallox vx(D1, D2); // RX & TX pins
unsigned long lastUpdated;

void setup() {
  Serial.begin(9600);

  vx.init();
  lastUpdated = vx.getUpdated();
  prettyPrint();

  Serial.println("Setup done.");
}

void loop() {

  // listen for data sent by mother boards or control panels
  vx.loop();

  unsigned long newUpdate = vx.getUpdated();
  if (lastUpdated != newUpdate) {
    // data hash changed
    lastUpdated = newUpdate;
    prettyPrint();
  }

  // write data
  int fanSpeed = vx.getFanSpeed();
  if (!anybodyHome() && vx.getFanSpeed() > 1) {
    vx.setFanSpeed(1);
  }
}

boolean anybodyHome() {
  return false;
}

void prettyPrint() {
  Serial.print("Inside temperature = "); Serial.println(vx.getInsideTemp());
  Serial.print("Outside temperature = "); Serial.println(vx.getOutsideTemp());
  Serial.print("Incoming temperature = "); Serial.println(vx.getIncomingTemp());
  Serial.print("Exhaust temperature = "); Serial.println(vx.getExhaustTemp());
  Serial.print("Fan speed = "); Serial.println(vx.getFanSpeed());
}

