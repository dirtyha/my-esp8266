/*
  CRCSwitch - Arduino libary for remote control outlet switches
  Copyright (c) 2011 Suat �zg�r.  All right reserved.

  Contributors:
  - Andre Koehler / info(at)tomate-online(dot)de
  - Gordeev Andrey Vladimirovich / gordeev(at)openpyro(dot)com
  - Skineffect / http://forum.ardumote.com/viewtopic.php?f=2&t=46
  - Dominik Fischer / dom_fischer(at)web(dot)de

  Project home: http://code.google.com/p/rc-switch/

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "RCSwitchCustom.h"

unsigned long CRCSwitch::nReceivedValue = NULL;
unsigned int CRCSwitch::nReceivedBitlength = 0;
unsigned int CRCSwitch::nReceivedDelay = 0;
unsigned int CRCSwitch::nReceivedProtocol = 0;
unsigned int CRCSwitch::timings[CRCSwitch_MAX_CHANGES];
int CRCSwitch::nReceiveTolerance = 60;

CRCSwitch::CRCSwitch() {
  this->nReceiverInterrupt = -1;
  this->nTransmitterPin = -1;
  CRCSwitch::nReceivedValue = NULL;
  this->setPulseLength(350);
  this->setRepeatTransmit(10);
  this->setReceiveTolerance(60);
  this->setProtocol(1);
}

/**
  * Sets the protocol to send.
  */
void CRCSwitch::setProtocol(int nProtocol) {
  this->nProtocol = nProtocol;
  if (nProtocol == 1){
	  this->setPulseLength(350);
  }
  else if (nProtocol == 2) {
	  this->setPulseLength(650);
  }
  //edit: Protokoll 4 für Rolladen mit Pulslänge 250ms und 4 Wiederholungen
  else if (nProtocol == 4) {
    this->setPulseLength(250);
    this->setRepeatTransmit(4);
  }
}

/**
  * Sets the protocol to send with pulse length in microseconds.
  */
void CRCSwitch::setProtocol(int nProtocol, int nPulseLength) {
  this->nProtocol = nProtocol;
  if (nProtocol == 1){
	  this->setPulseLength(nPulseLength);
  }
  else if (nProtocol == 2) {
	  this->setPulseLength(nPulseLength);
  }
  //edit: Protokoll 4 für Rolladen mit Pulslänge 250ms und 4 Wiederholungen
  else if (nProtocol == 4) {
    this->setPulseLength(nPulseLength);
  }
}


/**
  * Sets pulse length in microseconds
  */
void CRCSwitch::setPulseLength(int nPulseLength) {
  this->nPulseLength = nPulseLength;
}

/**
 * Sets Repeat Transmits
 */
void CRCSwitch::setRepeatTransmit(int nRepeatTransmit) {
  this->nRepeatTransmit = nRepeatTransmit;
}

/**
 * Set Receiving Tolerance
 */
void CRCSwitch::setReceiveTolerance(int nPercent) {
  CRCSwitch::nReceiveTolerance = nPercent;
}


/**
 * Enable transmissions
 *
 * @param nTransmitterPin    Arduino Pin to which the sender is connected to
 */
void CRCSwitch::enableTransmit(int nTransmitterPin) {
  this->nTransmitterPin = nTransmitterPin;
  pinMode(this->nTransmitterPin, OUTPUT);
}

/**
  * Disable transmissions
  */
void CRCSwitch::disableTransmit() {
  this->nTransmitterPin = -1;
}

/**
 * Switch a remote switch on (Type C Intertechno)
 *
 * @param sFamily  Familycode (a..f)
 * @param nGroup   Number of group (1..4)
 * @param nDevice  Number of device (1..4)
  */
void CRCSwitch::switchOn(char sFamily, int nGroup, int nDevice) {
  this->sendTriState( this->getCodeWordC(sFamily, nGroup, nDevice, true) );
}

/**
 * Switch a remote switch off (Type C Intertechno)
 *
 * @param sFamily  Familycode (a..f)
 * @param nGroup   Number of group (1..4)
 * @param nDevice  Number of device (1..4)
 */
void CRCSwitch::switchOff(char sFamily, int nGroup, int nDevice) {
  this->sendTriState( this->getCodeWordC(sFamily, nGroup, nDevice, false) );
}

/**
 * Switch a remote switch on (Type B with two rotary/sliding switches)
 *
 * @param nAddressCode  Number of the switch group (1..4)
 * @param nChannelCode  Number of the switch itself (1..4)
 */
void CRCSwitch::switchOn(int nAddressCode, int nChannelCode) {
  this->sendTriState( this->getCodeWordB(nAddressCode, nChannelCode, true) );
}

/**
 * Switch a remote switch off (Type B with two rotary/sliding switches)
 *
 * @param nAddressCode  Number of the switch group (1..4)
 * @param nChannelCode  Number of the switch itself (1..4)
 */
void CRCSwitch::switchOff(int nAddressCode, int nChannelCode) {
  this->sendTriState( this->getCodeWordB(nAddressCode, nChannelCode, false) );
}

/**
 * Deprecated, use switchOn(char* sGroup, char* sDevice) instead!
 * Switch a remote switch on (Type A with 10 pole DIP switches)
 *
 * @param sGroup        Code of the switch group (refers to DIP switches 1..5 where "1" = on and "0" = off, if all DIP switches are on it's "11111")
 * @param nChannelCode  Number of the switch itself (1..5)
 */
void CRCSwitch::switchOn(char* sGroup, int nChannel) {
  char* code[6] = { "00000", "10000", "01000", "00100", "00010", "00001" };
  this->switchOn(sGroup, code[nChannel]);
}

/**
 * Deprecated, use switchOff(char* sGroup, char* sDevice) instead!
 * Switch a remote switch off (Type A with 10 pole DIP switches)
 *
 * @param sGroup        Code of the switch group (refers to DIP switches 1..5 where "1" = on and "0" = off, if all DIP switches are on it's "11111")
 * @param nChannelCode  Number of the switch itself (1..5)
 */
void CRCSwitch::switchOff(char* sGroup, int nChannel) {
  char* code[6] = { "00000", "10000", "01000", "00100", "00010", "00001" };
  this->switchOff(sGroup, code[nChannel]);
}

/**
 * Switch a remote switch on (Type A with 10 pole DIP switches)
 *
 * @param sGroup        Code of the switch group (refers to DIP switches 1..5 where "1" = on and "0" = off, if all DIP switches are on it's "11111")
 * @param sDevice       Code of the switch device (refers to DIP switches 6..10 (A..E) where "1" = on and "0" = off, if all DIP switches are on it's "11111")
 */
void CRCSwitch::switchOn(char* sGroup, char* sDevice) {
	this->sendTriState( this->getCodeWordA(sGroup, sDevice, true) );
}

/**
 * Switch a remote switch off (Type A with 10 pole DIP switches)
 *
 * @param sGroup        Code of the switch group (refers to DIP switches 1..5 where "1" = on and "0" = off, if all DIP switches are on it's "11111")
 * @param sDevice       Code of the switch device (refers to DIP switches 6..10 (A..E) where "1" = on and "0" = off, if all DIP switches are on it's "11111")
 */
void CRCSwitch::switchOff(char* sGroup, char* sDevice) {
	this->sendTriState( this->getCodeWordA(sGroup, sDevice, false) );
}

/**
 * Returns a char[13], representing the Code Word to be send.
 * A Code Word consists of 9 address bits, 3 data bits and one sync bit but in our case only the first 8 address bits and the last 2 data bits were used.
 * A Code Bit can have 4 different states: "F" (floating), "0" (low), "1" (high), "S" (synchronous bit)
 *
 * +-------------------------------+--------------------------------+-----------------------------------------+-----------------------------------------+----------------------+------------+
 * | 4 bits address (switch group) | 4 bits address (switch number) | 1 bit address (not used, so never mind) | 1 bit address (not used, so never mind) | 2 data bits (on|off) | 1 sync bit |
 * | 1=0FFF 2=F0FF 3=FF0F 4=FFF0   | 1=0FFF 2=F0FF 3=FF0F 4=FFF0    | F                                       | F                                       | on=FF off=F0         | S          |
 * +-------------------------------+--------------------------------+-----------------------------------------+-----------------------------------------+----------------------+------------+
 *
 * @param nAddressCode  Number of the switch group (1..4)
 * @param nChannelCode  Number of the switch itself (1..4)
 * @param bStatus       Wether to switch on (true) or off (false)
 *
 * @return char[13]
 */
char* CRCSwitch::getCodeWordB(int nAddressCode, int nChannelCode, boolean bStatus) {
   int nReturnPos = 0;
   static char sReturn[13];

   char* code[5] = { "FFFF", "0FFF", "F0FF", "FF0F", "FFF0" };
   if (nAddressCode < 1 || nAddressCode > 4 || nChannelCode < 1 || nChannelCode > 4) {
    return '\0';
   }
   for (int i = 0; i<4; i++) {
     sReturn[nReturnPos++] = code[nAddressCode][i];
   }

   for (int i = 0; i<4; i++) {
     sReturn[nReturnPos++] = code[nChannelCode][i];
   }

   sReturn[nReturnPos++] = 'F';
   sReturn[nReturnPos++] = 'F';
   sReturn[nReturnPos++] = 'F';

   if (bStatus) {
      sReturn[nReturnPos++] = 'F';
   } else {
      sReturn[nReturnPos++] = '0';
   }

   sReturn[nReturnPos] = '\0';

   return sReturn;
}

/**
 * Returns a char[13], representing the Code Word to be send.
 *
 * getCodeWordA(char*, char*)
 *
 */
char* CRCSwitch::getCodeWordA(char* sGroup, char* sDevice, boolean bOn) {
	static char sDipSwitches[13];
    int i = 0;
	int j = 0;

	for (i=0; i < 5; i++) {
		if (sGroup[i] == '0') {
			sDipSwitches[j++] = 'F';
		} else {
			sDipSwitches[j++] = '0';
		}
	}

	for (i=0; i < 5; i++) {
		if (sDevice[i] == '0') {
			sDipSwitches[j++] = 'F';
		} else {
			sDipSwitches[j++] = '0';
		}
	}

	if ( bOn ) {
		sDipSwitches[j++] = '0';
		sDipSwitches[j++] = 'F';
	} else {
		sDipSwitches[j++] = 'F';
		sDipSwitches[j++] = '0';
	}

	sDipSwitches[j] = '\0';

	return sDipSwitches;
}

/**
 * Like getCodeWord (Type C = Intertechno)
 */
char* CRCSwitch::getCodeWordC(char sFamily, int nGroup, int nDevice, boolean bStatus) {
  static char sReturn[13];
  int nReturnPos = 0;

  if ( (byte)sFamily < 97 || (byte)sFamily > 112 || nGroup < 1 || nGroup > 4 || nDevice < 1 || nDevice > 4) {
    return '\0';
  }

  char* sDeviceGroupCode =  dec2binWzerofill(  (nDevice-1) + (nGroup-1)*4, 4  );
  char familycode[16][5] = { "0000", "F000", "0F00", "FF00", "00F0", "F0F0", "0FF0", "FFF0", "000F", "F00F", "0F0F", "FF0F", "00FF", "F0FF", "0FFF", "FFFF" };
  for (int i = 0; i<4; i++) {
    sReturn[nReturnPos++] = familycode[ (int)sFamily - 97 ][i];
  }
  for (int i = 0; i<4; i++) {
    sReturn[nReturnPos++] = (sDeviceGroupCode[3-i] == '1' ? 'F' : '0');
  }
  sReturn[nReturnPos++] = '0';
  sReturn[nReturnPos++] = 'F';
  sReturn[nReturnPos++] = 'F';
  if (bStatus) {
    sReturn[nReturnPos++] = 'F';
  } else {
    sReturn[nReturnPos++] = '0';
  }
  sReturn[nReturnPos] = '\0';
  return sReturn;
}

/**
 * Sends a Code Word
 * @param sCodeWord   /^[10FS]*$/  -> see getCodeWord
 */
void CRCSwitch::sendTriState(char* sCodeWord) {
  for (int nRepeat=0; nRepeat<nRepeatTransmit; nRepeat++) {
    int i = 0;
    while (sCodeWord[i] != '\0') {
      switch(sCodeWord[i]) {
        case '0':
          this->sendT0();
        break;
        case 'F':
          this->sendTF();
        break;
        case '1':
          this->sendT1();
        break;
      }
      i++;
    }
    this->sendSync();
  }
}

//edit: Quad-State-Befehl mit zusätzl. Q senden
void CRCSwitch::sendQuadState(char* sCodeWord) {
  for (int nRepeat=0; nRepeat<nRepeatTransmit; nRepeat++) {
    //Syn-Bit wird hier VOR Übertragung gesendet!
 this->sendSync();
 int i = 0;
    while (sCodeWord[i] != '\0') {
      switch(sCodeWord[i]) {
        case '0':
          this->sendT0();
        break;
        case 'F':
          this->sendTF();
        break;
        case '1':
          this->sendT1();
        break;
        case 'Q':
          this->sendQQ();
        break;
      }
      i++;
    }
  }
}
/**
 * edit: Sends a Quad-State "Q" Bit
 *            ___   _
 * Waveform: |   |_| |___
 */
void CRCSwitch::sendQQ() {
  this->transmit(3,1);
  this->transmit(1,3);
}


void CRCSwitch::send(unsigned long Code, unsigned int length) {
  this->send( this->dec2binWzerofill(Code, length) );
}

void CRCSwitch::send(char* sCodeWord) {
  for (int nRepeat=0; nRepeat<nRepeatTransmit; nRepeat++) {
    int i = 0;
    while (sCodeWord[i] != '\0') {
      switch(sCodeWord[i]) {
        case '0':
          this->send0();
        break;
        case '1':
          this->send1();
        break;
      }
      i++;
    }
    this->sendSync();
  }
}

void CRCSwitch::transmit(int nHighPulses, int nLowPulses) {
    boolean disabled_Receive = false;
    int nReceiverInterrupt_backup = nReceiverInterrupt;
    if (this->nTransmitterPin != -1) {
        if (this->nReceiverInterrupt != -1) {
            this->disableReceive();
            disabled_Receive = true;
        }
        digitalWrite(this->nTransmitterPin, HIGH);
        delayMicroseconds( this->nPulseLength * nHighPulses);
        digitalWrite(this->nTransmitterPin, LOW);
        delayMicroseconds( this->nPulseLength * nLowPulses);
        if(disabled_Receive){
            this->enableReceive(nReceiverInterrupt_backup);
        }
    }
}
/**
 * Sends a "0" Bit
 *                       _
 * Waveform Protocol 1: | |___
 *                       _
 * Waveform Protocol 2: | |__
 */
void CRCSwitch::send0() {
	if (this->nProtocol == 1){
		this->transmit(1,3);
	}
  else if (this->nProtocol == 2) {
		this->transmit(1,2);
	}
}

/**
 * Sends a "1" Bit
 *                       ___
 * Waveform Protocol 1: |   |_
 *                       __
 * Waveform Protocol 2: |  |_
 */
void CRCSwitch::send1() {
  	if (this->nProtocol == 1){
		this->transmit(3,1);
	}
	else if (this->nProtocol == 2) {
		this->transmit(2,1);
	}
}


/**
 * Sends a Tri-State "0" Bit
 *            _     _
 * Waveform: | |___| |___
 */
void CRCSwitch::sendT0() {
  this->transmit(1,3);
  this->transmit(1,3);
}

/**
 * Sends a Tri-State "1" Bit
 *            ___   ___
 * Waveform: |   |_|   |_
 */
void CRCSwitch::sendT1() {
  this->transmit(3,1);
  this->transmit(3,1);
}

/**
 * Sends a Tri-State "F" Bit
 *            _     ___
 * Waveform: | |___|   |_
 */
void CRCSwitch::sendTF() {
  this->transmit(1,3);
  this->transmit(3,1);
}

/**
 * Sends a "Sync" Bit
 *                       _
 * Waveform Protocol 1: | |_______________________________
 *                       _
 * Waveform Protocol 2: | |__________
 */
void CRCSwitch::sendSync() {

    if (this->nProtocol == 1){
		this->transmit(1,31);
	}
	else if (this->nProtocol == 2) {
		this->transmit(1,10);
	}
   //edit: Syn-Bit für Rolladen
   else if (this->nProtocol == 4) {
       this->transmit(18,6);
   }
}

/**
 * Enable receiving data
 */
void CRCSwitch::enableReceive(int interrupt) {
  this->nReceiverInterrupt = interrupt;
  this->enableReceive();
}

void CRCSwitch::enableReceive() {
  if (this->nReceiverInterrupt != -1) {
    CRCSwitch::nReceivedValue = NULL;
    CRCSwitch::nReceivedBitlength = NULL;
    attachInterrupt(this->nReceiverInterrupt, handleInterrupt, CHANGE);
  }
}

/**
 * Disable receiving data
 */
void CRCSwitch::disableReceive() {
  detachInterrupt(this->nReceiverInterrupt);
  this->nReceiverInterrupt = -1;
}

bool CRCSwitch::available() {
  return CRCSwitch::nReceivedValue != NULL;
}

void CRCSwitch::resetAvailable() {
  CRCSwitch::nReceivedValue = NULL;
}

unsigned long CRCSwitch::getReceivedValue() {
    return CRCSwitch::nReceivedValue;
}

unsigned int CRCSwitch::getReceivedBitlength() {
  return CRCSwitch::nReceivedBitlength;
}

unsigned int CRCSwitch::getReceivedDelay() {
  return CRCSwitch::nReceivedDelay;
}

unsigned int CRCSwitch::getReceivedProtocol() {
  return CRCSwitch::nReceivedProtocol;
}

unsigned int* CRCSwitch::getReceivedRawdata() {
    return CRCSwitch::timings;
}

/**
 *
 */
bool CRCSwitch::receiveProtocol1(unsigned int changeCount){

	  unsigned long code = 0;
      unsigned long delay = CRCSwitch::timings[0] / 31;
      unsigned long delayTolerance = delay * CRCSwitch::nReceiveTolerance * 0.01;

      for (int i = 1; i<changeCount ; i=i+2) {

          if (CRCSwitch::timings[i] > delay-delayTolerance && CRCSwitch::timings[i] < delay+delayTolerance && CRCSwitch::timings[i+1] > delay*3-delayTolerance && CRCSwitch::timings[i+1] < delay*3+delayTolerance) {
            code = code << 1;
          } else if (CRCSwitch::timings[i] > delay*3-delayTolerance && CRCSwitch::timings[i] < delay*3+delayTolerance && CRCSwitch::timings[i+1] > delay-delayTolerance && CRCSwitch::timings[i+1] < delay+delayTolerance) {
            code+=1;
            code = code << 1;
          } else {
            // Failed
            i = changeCount;
            code = 0;
          }
      }
      code = code >> 1;
    if (changeCount > 6) {    // ignore < 4bit values as there are no devices sending 4bit values => noise
      CRCSwitch::nReceivedValue = code;
      CRCSwitch::nReceivedBitlength = changeCount / 2;
      CRCSwitch::nReceivedDelay = delay;
	  CRCSwitch::nReceivedProtocol = 1;
    }

	if (code == 0){
		return false;
	}else if (code != 0){
		return true;
	}


}

bool CRCSwitch::receiveProtocol2(unsigned int changeCount){

	  unsigned long code = 0;
      unsigned long delay = CRCSwitch::timings[0] / 10;
      unsigned long delayTolerance = delay * CRCSwitch::nReceiveTolerance * 0.01;

      for (int i = 1; i<changeCount ; i=i+2) {

          if (CRCSwitch::timings[i] > delay-delayTolerance && CRCSwitch::timings[i] < delay+delayTolerance && CRCSwitch::timings[i+1] > delay*2-delayTolerance && CRCSwitch::timings[i+1] < delay*2+delayTolerance) {
            code = code << 1;
          } else if (CRCSwitch::timings[i] > delay*2-delayTolerance && CRCSwitch::timings[i] < delay*2+delayTolerance && CRCSwitch::timings[i+1] > delay-delayTolerance && CRCSwitch::timings[i+1] < delay+delayTolerance) {
            code+=1;
            code = code << 1;
          } else {
            // Failed
            i = changeCount;
            code = 0;
          }
      }
      code = code >> 1;
    if (changeCount > 6) {    // ignore < 4bit values as there are no devices sending 4bit values => noise
      CRCSwitch::nReceivedValue = code;
      CRCSwitch::nReceivedBitlength = changeCount / 2;
      CRCSwitch::nReceivedDelay = delay;
	  CRCSwitch::nReceivedProtocol = 2;
    }

	if (code == 0){
		return false;
	}else if (code != 0){
		return true;
	}

}
void CRCSwitch::handleInterrupt() {

  static unsigned int duration;
  static unsigned int changeCount;
  static unsigned long lastTime;
  static unsigned int repeatCount;


  long time = micros();
  duration = time - lastTime;

  if (duration > 5000 && duration > CRCSwitch::timings[0] - 200 && duration < CRCSwitch::timings[0] + 200) {
    repeatCount++;
    changeCount--;
    if (repeatCount == 2) {
		if (receiveProtocol1(changeCount) == false){
			if (receiveProtocol2(changeCount) == false){
				//failed
			}
		}
      repeatCount = 0;
    }
    changeCount = 0;
  } else if (duration > 5000) {
    changeCount = 0;
  }

  if (changeCount >= CRCSwitch_MAX_CHANGES) {
    changeCount = 0;
    repeatCount = 0;
  }
  CRCSwitch::timings[changeCount++] = duration;
  lastTime = time;
}

/**
  * Turns a decimal value to its binary representation
  */
char* CRCSwitch::dec2binWzerofill(unsigned long Dec, unsigned int bitLength){
  static char bin[64];
  unsigned int i=0;

  while (Dec > 0) {
    bin[32+i++] = ((Dec & 1) > 0) ? '1' : '0';
    Dec = Dec >> 1;
  }

  for (unsigned int j = 0; j< bitLength; j++) {
    if (j >= bitLength - i) {
      bin[j] = bin[ 31 + i - (j - (bitLength - i)) ];
    }else {
      bin[j] = '0';
    }
  }
  bin[bitLength] = '\0';

  return bin;
}
