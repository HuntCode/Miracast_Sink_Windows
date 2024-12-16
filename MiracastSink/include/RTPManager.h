#ifndef RTP_MANAGER_H
#define RTP_MANAGER_H

#include <hv/UdpServer.h>

#include "IMiracastCallback.h"
#include "TSDecoder.h"

#define TS_TYPE_VIDEO 1
#define TS_TYPE_AUDIO 2

const uint16_t MAX_SEQUENCE_NUMBER = 65535;
#if defined(WIN32)
const int PACKET_LOSS_THRESHOLD = 500;
#elif defined(ANDROID)
const int PACKET_LOSS_THRESHOLD = 50;
#endif

struct RTPPacket {
    uint16_t sequenceNumber;
    uint8_t* data;
    int length;


    bool isVideoPacket() {
        if (data == nullptr) {
            return false;
        }
        return true;
    };

};

class RTPManager;
class RTPPacketHandler {
public:
    RTPPacketHandler(RTPManager* owner, std::function<void(uint16_t)> packetLossCallback);
    ~RTPPacketHandler();

    void addPayload(const uint8_t *packet, size_t length);
    void reset();

private:
    RTPManager* m_owner;
    std::vector<RTPPacket> sortedPackets;
    std::vector<RTPPacket> cachedPackets;
    uint16_t expectedSequenceNumber;
    std::function<void(uint16_t)> onPacketLoss;

    std::mutex bufferMutex;
    std::condition_variable cv;
    std::thread consumerThread;
    bool stopProcessing;


    void consumePackets();
    uint16_t previousSequenceNumber(uint16_t seqNum) const;
    uint16_t nextSequenceNumber(uint16_t seqNum) const;
private:

};

class RTSPManager;
class RTPManager
{
public:
	RTPManager(RTSPManager* owner);
	~RTPManager();

	void Start();
	void Stop();

    void SetSourceIP(std::string& sourceIP);
	uint16_t GetRTPPort();
    void SetRTCPServerPort(uint16_t port);

	void SetMiracastCallback(std::shared_ptr<IMiracastCallback> callback);

	void SetStreamID(uint32_t streamId);

    /* IMiracastCallback */
	void OnData(int type, uint8_t* buffer, int bufSize);

    void InputTSData(const uint8_t* data, size_t bytes);

    void initTsDecoder();

    void resetTsDecoder();

    void RequestIr();

    void SendRTPFBNACK(uint16_t pid, uint16_t blp);
private:
    RTSPManager* m_owner;
	hv::UdpServer m_rtpServer;
	hv::UdpServer m_rtcpServer;
    std::string m_sourceIP;
	uint16_t m_rtpPort = 0;
    uint16_t m_rtcpServerPort = 0;
    uint32_t m_streamId;
    uint32_t m_selfSSRC = 0;
    uint32_t m_mediaSSRC = 0;

	std::shared_ptr<IMiracastCallback> m_miracastCallback;
    std::shared_ptr<RTPPacketHandler> m_rtpPacketHandler;

	FILE* fTS; // for dump ts file
	FILE* fH264; // for dump ts file

	std::shared_ptr<mediakit::TSDecoder> m_tsDecoder;

	bool GenerateRtpPort(uint16_t& port);
	void OnPayloadData(const hv::SocketChannelPtr& channel, hv::Buffer* msg);
	void OnRTCPData(const hv::SocketChannelPtr& channel, hv::Buffer* msg);

    void OnPacketLoss(uint16_t seqNumber);
public:
    uint64_t lastRequestIrTime = 0;
    uint64_t lastKeyFrameTime = 0;
};

#endif // RTP_MANAGER_H