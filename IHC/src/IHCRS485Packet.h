#ifndef IHCRS485PACKET_H
#define IHCRS485PACKET_H
#include <Vector.h>

#define MAX_PACKET_SIZE 21
#define MAX_DATA_SIZE 16

namespace IHCDefs
{
// Transmission bytes
const byte SOH = 0x01;
const byte STX = 0x02;
const byte ACK = 0x06;
const byte ETB = 0x17;

// Commands
const byte DATA_READY = 0x30;
const byte SET_OUTPUT = 0x7A;
const byte GET_OUTPUTS = 0x82;
const byte OUTP_STATE = 0x83;
const byte GET_INPUTS = 0x86;
const byte INP_STATE = 0x87;
const byte ACT_INPUT = 0x88;

// Receiver IDs
const byte ID_DISPLAY = 0x09;
const byte ID_MODEM = 0x0A;
const byte ID_IHC = 0x12;
const byte ID_AC = 0x1B;
const byte ID_PC = 0x1C;
const byte ID_PC2 = 0x1D;
}; // namespace IHCDefs

class IHCRS485Packet
{
public:
	IHCRS485Packet()
		: mId(0x00),
		  mDataType(0x00),
		  mIsComplete(false)
	{
		mPacket.setStorage(mPacketStorage);
		mData.setStorage(mDataStorage);
	};

	IHCRS485Packet(byte id,
				   byte dataType)
	{
		mPacket.setStorage(mPacketStorage);
		mData.setStorage(mDataStorage);

		setData(id, dataType, NULL);
	};

	void fromBuffer(Vector<byte> *pBuffer)
	{
		if (pBuffer != NULL)
		{
			mIsComplete = false;
			mPacket.clear();
			mData.clear();

			if (pBuffer->size() > 2 && (*pBuffer)[0] == IHCDefs::STX)
			{
				mId = (*pBuffer)[1];
				mDataType = (*pBuffer)[2];
				byte crc = 0x00;
				for (unsigned int j = 0; j < 3; j++)
				{
					crc += (*pBuffer)[j];
					mPacket.push_back((*pBuffer)[j]);
				}
				for (unsigned int j = 3; j < pBuffer->size(); j++)
				{
					crc += (*pBuffer)[j];
					mPacket.push_back((*pBuffer)[j]);
					if ((*pBuffer)[j] != IHCDefs::ETB)
					{
						mData.push_back((*pBuffer)[j]);
					}
					else
					{
						if ((j + 1) == (pBuffer->size() - 1))
						{
							if ((*pBuffer)[j + 1] == (byte)(crc & 0xFF))
							{
								mPacket.push_back((*pBuffer)[j + 1]);
								mIsComplete = true;
								break;
							}
						}
					}
				}
			}
		}
	};

	void setData(byte id, byte dataType, Vector<byte> *pData)
	{
		mId = id;
		mDataType = dataType;

		mPacket.clear();
		mData.clear();

		mPacket.push_back(IHCDefs::STX);
		mPacket.push_back(id);
		mPacket.push_back(dataType);

		if (pData != NULL)
		{
			for (unsigned int i = 0; i < pData->size(); i++)
			{
				mPacket.push_back((*pData)[i]);
				mData.push_back((*pData)[i]);
			}
		}

		mPacket.push_back(IHCDefs::ETB);
		byte crc = 0;
		for (unsigned int icrc = 0; icrc < mPacket.size(); icrc++)
		{
			crc += mPacket[icrc];
		}
		mPacket.push_back((byte)(crc & 0xFF));
		mIsComplete = true;
	}

	void printVector(Vector<byte> *pBuffer)
	{
		if (pBuffer != NULL)
		{
			Serial.print("Bytes: ");
			for (unsigned int j = 0; j < pBuffer->size(); j++)
			{
				if (j != 0)
				{
					Serial.print(",");
				}
				Serial.print((*pBuffer)[j], HEX);
			}
			Serial.println();
		}
	};

	void prettyPrint()
	{
		String receiver;
		switch (mId)
		{
		case IHCDefs::ID_MODEM:
			receiver = "MODEM";
			break;
		case IHCDefs::ID_DISPLAY:
			receiver = "DISPLAY";
			break;
		case IHCDefs::ID_IHC:
			receiver = "IHC";
			break;
		case IHCDefs::ID_AC:
			receiver = "AC";
			break;
		case IHCDefs::ID_PC:
			receiver = "PC";
			break;
		case IHCDefs::ID_PC2:
			receiver = "PC2";
			break;
		default:
			receiver = "UNKNOWN";
			break;
		}
		Serial.print("Receiver: ");
		Serial.println(receiver.c_str());
		Serial.print("Data type: ");
		Serial.println(mDataType, HEX);
		Serial.print("Is complete: ");
		Serial.println(mIsComplete);
		printVector(&mPacket);
	};

	bool isComplete()
	{
		return mIsComplete;
	};

	Vector<byte> *getBuffer()
	{
		return &mPacket;
	};

	Vector<byte> *getData()
	{
		return &mData;
	};

	byte getID()
	{
		return mId;
	};

	byte getDataType()
	{
		return mDataType;
	};

private:
	byte mId;
	byte mDataType;
	bool mIsComplete;
	Vector<byte> mPacket;
	byte mPacketStorage[MAX_PACKET_SIZE];
	Vector<byte> mData;
	byte mDataStorage[MAX_DATA_SIZE];
};

#endif /* IHCRS485PACKET_H */
