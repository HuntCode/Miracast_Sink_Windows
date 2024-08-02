#ifndef RTP_MANAGER_H
#define RTP_MANAGER_H

#include <hv/UdpServer.h>

#include "WXBase.h"
#include "IMiracastCallback.h"

class RTPManager;
class TSParseWorker : public WXThread {
public:
	TSParseWorker(RTPManager* owner);

	void ThreadPrepare() override;
	void ThreadProcess() override;
	void ThreadPost() override;

private:
	RTPManager* m_owner;
	void* m_pTSDemux;
	FILE* fH264; // for dump test file
	FILE* fAAC;  // for dump test file
};

class RTPManager
{
public:
	RTPManager();
	~RTPManager();

	void Start();
	void Stop();

	uint16_t GetRTPPort();

	WXFifo& TSDataBuffer();

	void SetMiracastCallback(std::shared_ptr<IMiracastCallback> callback);

	void SetStreamID(uint32_t streamId);

	void OnData(int type, uint8_t* buffer, int bufSize);

private:
	hv::UdpServer m_rtpServer;
	uint16_t m_rtpPort = 0;
	WXFifo m_tsDataBuffer;
	std::shared_ptr<IMiracastCallback> m_miracastCallback;
	uint32_t m_streamId;

	bool GenerateRtpPort(uint16_t& port);
	void OnPayloadData(const hv::SocketChannelPtr& channel, hv::Buffer* msg);

	std::unique_ptr<TSParseWorker> m_tsParseWorker;
};

#endif // RTP_MANAGER_H