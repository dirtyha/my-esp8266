#include <Vallox.h>

Vallox vx(D1, D2); // RX & TX pins
unsigned long lastUpdate = 0;
unsigned long lastCheck = 0;

void setup() {
  Serial.begin(9600);

  vx.init();

  Serial.println("Setup done.");
}

void loop() {
  // listen for data sent by mother boards or control panels
  vx.loop();

  unsigned long newUpdate = vx.getUpdated();
  if (lastUpdate != newUpdate) {
    // data has changed
    lastUpdate = newUpdate;
    prettyPrint();
  }

  // send data from arduino to Vallox bus
  // setters follow pattern set<variable_name>()
  // see prettyPrint for available variables
  unsigned long now = millis();
  if (now - lastCheck > 60000 && !anybodyHome() && vx.getFanSpeed() > 1) {
	lastCheck = now;
    vx.setFanSpeed(1);
  }
}

// mock
boolean anybodyHome() {
  return false;
}

// getters follow pattern get<variable_name>()
void prettyPrint() {
  Serial.print("Inside temperature (C) = "); Serial.println(vx.getInsideTemp());
  Serial.print("Outside temperature (C) = "); Serial.println(vx.getOutsideTemp());
  Serial.print("Incoming temperature (C) = "); Serial.println(vx.getIncomingTemp());
  Serial.print("Exhaust temperature (C) = "); Serial.println(vx.getExhaustTemp());
  Serial.print("Fan speed (1-8) = "); Serial.println(vx.getFanSpeed());
  Serial.print("Power = "); vx.isOn() ? Serial.println("on") : Serial.println("off");
  Serial.print("RH mode = "); vx.isRhMode() ? Serial.println("on") : Serial.println("off");
  Serial.print("Heating mode = "); vx.isHeatingMode() ? Serial.println("on") : Serial.println("off");
  Serial.print("Summer mode = "); vx.isSummerMode() ? Serial.println("on") : Serial.println("off");
  Serial.print("Is filter dirty = "); vx.isFilter() ? Serial.println("yes") : Serial.println("no");
  Serial.print("Is heating = "); vx.isHeating() ? Serial.println("yes") : Serial.println("no");
  Serial.print("Is faulty = "); vx.isFault() ? Serial.println("yes") : Serial.println("no");
  Serial.print("Needs service = "); vx.isService() ? Serial.println("yes") : Serial.println("no");
  Serial.print("Default fan speed (1-8) = "); Serial.println(vx.getDefaultFanSpeed());
  Serial.print("RH (%) = "); Serial.println(vx.getRh());
  Serial.print("Service period (months) = "); Serial.println(vx.getServicePeriod());
  Serial.print("Service count down (months) = "); Serial.println(vx.getServiceCounter());
  Serial.print("Heating target (C) = "); Serial.println(vx.getHeatingTarget());
  Serial.println();
}

