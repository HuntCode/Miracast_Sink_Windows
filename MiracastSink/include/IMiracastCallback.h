#ifndef IMIRACAST_CALLBACK_H
#define IMIRACAST_CALLBACK_H


class IMiracastCallback {
public:
	virtual void OnConnect(uint32_t streamId) = 0;
	virtual void OnDisconnect(uint32_t streamId) = 0;
	virtual void OnPlay(uint32_t streamId) = 0;
	virtual void OnData(uint32_t streamId, int type, uint8_t* buffer, int bufSize) = 0;
};

#endif // IMIRACAST_CALLBACK_H
