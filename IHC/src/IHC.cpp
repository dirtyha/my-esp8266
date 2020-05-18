#include "IHC.h"

IHCRS485Packet getOutputPacket(IHCDefs::ID_IHC, IHCDefs::GET_OUTPUTS);

IHC::IHC()
{
}

void IHC::init(SoftwareSerial *pSerial, bool debug)
{
	mDebug = debug;
	mpSendPacket = NULL;
	mLastPolled = millis();
	mAvailable = false;
	mpSerial = pSerial;
	mpSerial->begin(19200);

	if (mDebug)
	{
		Serial.println("IHC: Init.");
	}
}

int IHC::getStatus()
{
	unsigned long now = millis();
	if (now - mLastPolled > 20000)
	{
		return -1;
	}

	return 1;
}

IHCRS485Packet *IHC::receive()
{
	if (mAvailable)
	{
		mAvailable = false;
		return &mReceivedPacket;
	}
	else
	{
		return NULL;
	}
}

void IHC::send(IHCRS485Packet *pPacket)
{
	mpSendPacket = pPacket;
}

void IHC::loop()
{
	// Clean start - make receiving buffers clean
	purge();

	IHCRS485Packet *pPacket = readPacket();
	//	if (pPacket->isComplete() && (pPacket->getID() == IHCDefs::ID_PC || pPacket->getID() == IHCDefs::ID_PC2))
	if (pPacket->isComplete() && (pPacket->getID() == IHCDefs::ID_PC))
	{
		switch (pPacket->getDataType())
		{
		case IHCDefs::ACK:
			mAvailable = false;
			break;

		case IHCDefs::DATA_READY:
			mAvailable = false;
			// Ready to write something to IHC
			if (mpSendPacket != NULL)
			{
				// Send user message
				writePacket(mpSendPacket);
				mpSendPacket = NULL;
			}
			else
			{
				unsigned long now = millis();
				if (now - mLastPolled > POLLING_INTERVAL)
				{
					mLastPolled = now;
					writePacket(&getOutputPacket);
					readReply();
				}
			}
			break;
		}
	}
}

void IHC::readReply()
{
	while (1)
	{
		if (millis() - mLastPolled > 5000)
		{
			return;
		}

		IHCRS485Packet *pPacket = readPacket();
		if (pPacket->isComplete() && (pPacket->getID() == IHCDefs::ID_PC) && pPacket->getDataType() == IHCDefs::OUTP_STATE)
		{
			mAvailable = true;
			return;
		}
	}
}

IHCRS485Packet *IHC::readPacket()
{
	byte storage[MAX_PACKET_SIZE];
	Vector<byte> buffer(storage);

	byte c = readByte();
	while (c != IHCDefs::STX)
	{
		// Discard bytes and sync with STX
		c = readByte();
	}

	buffer.push_back(c); // STX

	for (int i = 1; i < MAX_PACKET_SIZE; i++)
	{
		c = readByte();
		buffer.push_back(c);
		if (c == IHCDefs::ETB)
		{
			c = readByte();
			buffer.push_back(c); // CRC
			break;
		}
	}

	mReceivedPacket.fromBuffer(&buffer);

	if (mDebug)
	{
		Serial.println("Received ==>");
		mReceivedPacket.prettyPrint();
		Serial.println("<== Received");
	}

	return &mReceivedPacket;
}

byte IHC::readByte()
{
	while (true)
	{
		if (mpSerial->available() > 0)
		{
			return mpSerial->read();
		}
//		delay(10);
	}
}

void IHC::purge()
{
	while (mpSerial->available() > 0)
	{
		mpSerial->read();
	}
}

void IHC::writePacket(IHCRS485Packet *pPacket)
{
	if (pPacket != NULL)
	{
		Vector<byte> *buffer = pPacket->getBuffer();
		for (int i = 0; i < buffer->size(); i++)
		{
			mpSerial->write((*buffer)[i]);
		}

		if (mDebug)
		{
			Serial.println("Sent ==>");
			pPacket->prettyPrint();
			Serial.println("<== Sent");
		}
	}
}
