#ifndef IHCIO_H
#define IHCIO_H

class IHCIO
{
public:
	IHCIO(String name, unsigned short module, unsigned short port, bool isOutput = true) : mName(name),
																						   mModule(module),
																						   mPort(port),
																						   mIsOutput(isOutput),
																						   mState(false),
																						   mChangeId(0)
	{
	}

	String getName()
	{
		return mName;
	}

	unsigned short getModule()
	{
		return mModule;
	}

	unsigned short getPort()
	{
		return mPort;
	}

	bool isOutput()
	{
		return mIsOutput;
	}

	bool getState()
	{
		return mState;
	}

	bool setState(bool state, unsigned long changeId)
	{
		bool ret = false;

		if (mChangeId == 0 || mState != state)
		{
			ret = true;
			mState = state;
			if (changeId != 0)
			{
				mChangeId = changeId;
			}
			else
			{
				mChangeId = millis();
			}
		}

		return ret;
	}

	unsigned long getChangeId()
	{
		return mChangeId;
	}

private:
	String mName;
	bool mIsOutput;
	unsigned short mModule;
	unsigned short mPort;
	bool mState;
	unsigned long mChangeId;
};

#endif /* IHCIO_H */
