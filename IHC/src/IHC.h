#ifndef IHC_H
#define IHC_H

#include <sys/types.h>
#include <SoftwareSerial.h>
#include "IHCRS485Packet.h"

#define MAX_QUEUE_SIZE 10
#define POLLING_INTERVAL 10000

class IHC
{
public:
	IHC();
	void init(SoftwareSerial *pSerial, bool debug);
	void loop();
	IHCRS485Packet *receive();
	void send(IHCRS485Packet *pPacket);
	int getStatus();

private:
	IHCRS485Packet *readPacket();
	void writePacket(IHCRS485Packet *pPacket);
	void readReply();
	byte readByte();
	void purge();

	bool mDebug;
	SoftwareSerial *mpSerial;
	IHCRS485Packet mReceivedPacket;
	IHCRS485Packet *mpSendPacket;
	unsigned long mLastPolled;
	bool mAvailable;
};

#endif /* IHC_H */
