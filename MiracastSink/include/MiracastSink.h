#ifndef MIRACAST_SINK_H
#define MIRACAST_SINK_H

#include "RTSPManager.h"
#include "IMiracastCallback.h"

class MiracastSink
{
public:
	MiracastSink();
	~MiracastSink();

	bool Start(const std::string &ip);
	void Stop();
	void SetDeviceName(std::string name);
	void SetStreamID(uint32_t streamId);
	void SetMiracastCallback(std::shared_ptr<IMiracastCallback> callback);

	void SendHIDMouse(unsigned char type, char xdiff, char ydiff);
	void SendHIDKeyboard(unsigned char type, unsigned char modType, unsigned short keyboardValue);

private:
	std::shared_ptr<RTSPManager> m_rtspManager;
};

#endif // MIRACAST_SINK_H