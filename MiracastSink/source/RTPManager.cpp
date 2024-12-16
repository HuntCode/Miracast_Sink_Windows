#include <iostream>
#include <random>

#include "RTPManager.h"
#include "RTSPManager.h"
#include "Util/logger.h"
#include <chrono>


/*
* create ssrc
*/
#if defined(OS_WINDOWS) || defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#include <wincrypt.h>

uint32_t rtp_ssrc(void)
{
	uint32_t seed;
	HCRYPTPROV provider;

	seed = (uint32_t)rand();
	if (CryptAcquireContext(&provider, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT | CRYPT_SILENT)) {
		CryptGenRandom(provider, sizeof(seed), (PBYTE)&seed);
		CryptReleaseContext(provider, 0);
	}
	return seed;
}

#elif defined(OS_LINUX) || defined(OS_MAC)
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

static int read_random(uint32_t* dst, const char* file)
{
	int fd = open(file, O_RDONLY);
	int err = -1;
	if (fd == -1)
		return -1;
	err = (int)read(fd, dst, sizeof(*dst));
	close(fd);
	return err;
}
uint32_t rtp_ssrc(void)
{
	uint32_t seed;
	if (read_random(&seed, "/dev/urandom") == sizeof(seed))
		return seed;
	if (read_random(&seed, "/dev/random") == sizeof(seed))
		return seed;
	return (uint32_t)rand();
}
#else

uint32_t rtp_ssrc(void)
{
	return rand();
}

#endif


// From WebRTC module_common_types_public.h
template <typename U>
inline bool IsNewer(U value, U prev_value) {
	static_assert(!std::numeric_limits<U>::is_signed, "U must be unsigned");
	// kBreakpoint is the half-way mark for the type U. For instance, for a
	// uint16_t it will be 0x8000, and for a uint32_t, it will be 0x8000000.
	constexpr U kBreakpoint = (std::numeric_limits<U>::max() >> 1) + 1;
	// Distinguish between elements that are exactly kBreakpoint apart.
	// If t1>t2 and |t1-t2| = kBreakpoint: IsNewer(t1,t2)=true,
	// IsNewer(t2,t1)=false
	// rather than having IsNewer(t1,t2) = IsNewer(t2,t1) = false.
	if (value - prev_value == kBreakpoint) {
		return value > prev_value;
	}
	return value != prev_value &&
		static_cast<U>(value - prev_value) < kBreakpoint;
}

// NB: Doesn't fulfill strict weak ordering requirements.
//     Mustn't be used as std::map Compare function.
inline bool IsNewerSequenceNumber(uint16_t sequence_number, uint16_t prev_sequence_number) {
	return IsNewer(sequence_number, prev_sequence_number);
}


RTPPacketHandler::RTPPacketHandler(RTPManager* owner, std::function<void(uint16_t)> packetLossCallback) :
	m_owner(owner), expectedSequenceNumber(0), onPacketLoss(packetLossCallback), stopProcessing(false)
{
	consumerThread = std::thread(&RTPPacketHandler::consumePackets, this);
}

RTPPacketHandler::~RTPPacketHandler()
{
    {
		std::lock_guard<std::mutex> lock(bufferMutex);
		stopProcessing = true;
	}
	cv.notify_all(); // 通知所有线程退出
	if (consumerThread.joinable()) consumerThread.join();
}

void RTPPacketHandler::addPayload(const uint8_t *packet, size_t length)
{
	// 获取sequnceNumber
	uint16_t sequenceNumber = (packet[2] << 8) | packet[3];
	RTPPacket rtpPacket;
	rtpPacket.length = length;
	rtpPacket.sequenceNumber = sequenceNumber;
	rtpPacket.data = (uint8_t*)malloc(length * sizeof(uint8_t));
	memcpy(rtpPacket.data, packet, length * sizeof(uint8_t));


	std::lock_guard<std::mutex> lock(bufferMutex);
	cachedPackets.push_back(rtpPacket);
	cv.notify_all();

}

void RTPPacketHandler::consumePackets() {
	uint16_t nextSequNumber = 0;

	while (!stopProcessing) {
		RTPPacket frontPacket;

		//从cached package当中读取数据
		{
			std::unique_lock<std::mutex> lock(bufferMutex);
			if (cachedPackets.empty()) {
				cv.wait(lock);
			}

			// 如果stop了，直接return
			if (stopProcessing) {
				break;
			}

			frontPacket = cachedPackets.front();
			cachedPackets.erase(cachedPackets.begin());
		}


		// 做一个排序，插入到对应的地方
		auto it = std::lower_bound(sortedPackets.begin(), sortedPackets.end(), frontPacket,
			[](const RTPPacket& a, const RTPPacket& b) {
				return IsNewerSequenceNumber(b.sequenceNumber, a.sequenceNumber);
			});

		// 检查是否已经存在同序号的数据包
		if (it != sortedPackets.end() && it->sequenceNumber == frontPacket.sequenceNumber) {
			delete[] frontPacket.data;
			continue;
		}

		// 插入数据包
		sortedPackets.insert(it, frontPacket);


		while (true) {
			RTPPacket* avaliblePackage = nullptr;

			// 如果当前是空的，那么继续
			if (sortedPackets.empty()) {
				break;
			}

			// 获取第一个数据包
			RTPPacket package = sortedPackets.front();
			if (package.sequenceNumber == nextSequNumber) {
				avaliblePackage = &package;
				nextSequNumber = nextSequenceNumber(package.sequenceNumber);
				sortedPackets.erase(sortedPackets.begin());
			}
			else {
				// 如果当前包的seq小于nextseq，那么直接丢弃包
				if (IsNewerSequenceNumber(nextSequNumber, package.sequenceNumber)) {
					sortedPackets.erase(sortedPackets.begin());
					free(package.data);
					continue;
				}

				// 检查排序队列中丢包情况，发送NACK
				if (sortedPackets.size() >= (PACKET_LOSS_THRESHOLD / 2)) {
					uint16_t blp = 0;
#if defined(WIN32)
					// Windows性能比Android好，可以判断后续16包丢包情况
					// Android端只发单丢包请求
					for (const auto& packet : sortedPackets) {
						if (packet.sequenceNumber > nextSequNumber && 
							packet.sequenceNumber <= nextSequNumber + 16) {
							blp |= (1 << (packet.sequenceNumber - (nextSequNumber + 1)));
						}
					
						if (packet.sequenceNumber > nextSequNumber + 16) break;
					}

					blp = ~blp;
#endif

					if (m_owner) {
						m_owner->SendRTPFBNACK(nextSequNumber, blp);
					}
				}
				

				// 当前的缓存包没有达到最大长度，那么继续等待下一个包过来
				if (sortedPackets.size() < PACKET_LOSS_THRESHOLD) {
					break;
				}

				//如果缓存包已经达到了最大长度，那么清除一个继续
				nextSequNumber = nextSequenceNumber(package.sequenceNumber);
				avaliblePackage = &package;
				sortedPackets.erase(sortedPackets.begin());
			}

			// 如果拿到的是一个空的，
			if (avaliblePackage == nullptr) {
				break;
			}

			int length = avaliblePackage->length - 12;
			uint8_t* startedPos = avaliblePackage->data + 12;

			// 表示是一个错误的包
			if (length % 188 != 0) {
				continue;
			}

			int  packetSize = length / 188;
			for (int i = 0; i < packetSize; i++) {
				if (m_owner) {
					m_owner->InputTSData(startedPos + i * 188, 188);
				}
			}
			free(avaliblePackage->data);
		}
	}
	DebugL << "consumerThread exit..." << sortedPackets.size();		
}

uint16_t RTPPacketHandler::previousSequenceNumber(uint16_t seqNum) const
{
	return (seqNum == 0) ? MAX_SEQUENCE_NUMBER : (seqNum - 1);
}

uint16_t RTPPacketHandler::nextSequenceNumber(uint16_t seqNum) const
{
	return (seqNum + 1) % (MAX_SEQUENCE_NUMBER + 1);
}

void RTPPacketHandler::reset() {
	sortedPackets.clear();
}

/*
 *	RTPManager
 */
RTPManager::RTPManager(RTSPManager* owner):
	m_owner(owner), m_streamId(0), 
	m_rtpPacketHandler(nullptr), m_tsDecoder(nullptr), fTS(nullptr)
{
	lastRequestIrTime = 0;
	m_selfSSRC = rtp_ssrc();
}

RTPManager::~RTPManager()
{

}


void RTPManager::resetTsDecoder() {

}

void RTPManager::RequestIr() {
	if (m_owner != nullptr) {
		m_owner->RequestIdr();
	}
}

void RTPManager::SendRTPFBNACK(uint16_t pid, uint16_t blp)
{
	uint8_t RTP_FB_NACK_Packet[16];

	struct sockaddr_in addr = { 0 };
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(m_sourceIP.c_str());
	addr.sin_port = htons(m_rtcpServerPort);

	RTP_FB_NACK_Packet[0] = 0x80 | 1;  // version=2, Generic NACK
	RTP_FB_NACK_Packet[1] = 205;       // RTPFB
	RTP_FB_NACK_Packet[2] = 0;
	RTP_FB_NACK_Packet[3] = 3;         //length = 3

	// SSRC of packet sender
	RTP_FB_NACK_Packet[4] = (m_selfSSRC >> 24) & 0xff;
	RTP_FB_NACK_Packet[5] = (m_selfSSRC >> 16) & 0xff;
	RTP_FB_NACK_Packet[6] = (m_selfSSRC >> 8) & 0xff;
	RTP_FB_NACK_Packet[7] =  m_selfSSRC & 0xff;

	//SSRC of media source
	RTP_FB_NACK_Packet[8] = (m_mediaSSRC >> 24) & 0xff;
	RTP_FB_NACK_Packet[9] = (m_mediaSSRC >> 16) & 0xff;
	RTP_FB_NACK_Packet[10] = (m_mediaSSRC >> 8) & 0xff;
	RTP_FB_NACK_Packet[11] = (m_mediaSSRC & 0xff);

	//lost packet ID
	RTP_FB_NACK_Packet[12] = (pid >> 8) & 0xff;
	RTP_FB_NACK_Packet[13] = (pid & 0xff);

	//BLP
	RTP_FB_NACK_Packet[14] = (blp >> 8) & 0xff;
	RTP_FB_NACK_Packet[15] = (blp & 0xff);

	m_rtcpServer.sendto(RTP_FB_NACK_Packet, sizeof(RTP_FB_NACK_Packet), (struct sockaddr*)&addr);
}


void RTPManager::initTsDecoder() {
	m_tsDecoder = std::make_shared<mediakit::TSDecoder>();
	m_tsDecoder->setOnDecode([this](int stream, int codecid, int flags, int64_t pts, int64_t dts, const void* data, size_t bytes) {
		if (codecid == 0x0F) {
			OnData(TS_TYPE_AUDIO, (uint8_t*)data, bytes);
		}
		else if (codecid == 0x1B) {
			//std::cout << "nVideoSize =" << bytes << std::endl;
			OnData(TS_TYPE_VIDEO, (uint8_t*)data, bytes);

			// 超过一定时间没有收到KeyFrame，需要请求关键帧，避免长时间花屏
			// 注意：Android端本身会3秒发一次关键帧，Windows端默认第一帧是关键帧，后面不会定时发关键帧
			auto now = std::chrono::high_resolution_clock::now();
			auto duration_in_seconds = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch());
			uint64_t currentTime = duration_in_seconds.count();
			if (flags & 0x0001) { //关键帧
				lastKeyFrameTime = currentTime;			
			}
			else { //非关键帧
				if (currentTime - lastKeyFrameTime > 5) {
					this->RequestIr();
					lastKeyFrameTime = currentTime; //避免Windows端回调过快，频繁发请求
					lastRequestIrTime = currentTime;
				}
			}
		}
		});

	m_tsDecoder->setOnPacketLoss([this](int type) {
			// 表示是video
			if (type == 0x1B) {				
				auto now = std::chrono::high_resolution_clock::now();
				auto duration_in_seconds = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch());
				uint64_t currentTime = duration_in_seconds.count();

				if (this->lastRequestIrTime == 0) {
					this->lastRequestIrTime = currentTime;
					this->RequestIr();
				}
				else {
					if (currentTime - this->lastRequestIrTime > 5) {
						this->RequestIr();
						this->lastRequestIrTime = currentTime;
					}
				}
			}
		});
}


void RTPManager::Start()
{
#if defined(HHDUMP_TEST)
#if defined(WIN32)
	//fTS = fopen("E:\\A_ReservedLand\\output.ts", "wb+");
#elif defined(ANDROID)
	std::string dumpFileName = "/sdcard/dump" + std::to_string(m_streamId);
	std::string dumpTS = dumpFileName + ".ts";
	fTS = fopen(dumpTS.c_str(), "wb+");
	if (fTS == nullptr)
	{
		DebugL << "fopen failed!!!";
	}
	std::string dumpH264 = dumpFileName + ".h264";
	fH264 = fopen(dumpH264.c_str(), "wb+");
#endif
#endif

	if (m_rtpPacketHandler == nullptr) {
		m_rtpPacketHandler = std::make_shared<RTPPacketHandler>(this,
			std::bind(&RTPManager::OnPacketLoss, this, std::placeholders::_1));
	}

	initTsDecoder();

	GenerateRtpPort(m_rtpPort);
	m_rtpServer.createsocket(m_rtpPort);
	m_rtpServer.onMessage = std::bind(&RTPManager::OnPayloadData, this, std::placeholders::_1, std::placeholders::_2);
	m_rtpServer.start();

	m_rtcpServer.createsocket(m_rtpPort + 1);
	m_rtcpServer.onMessage = std::bind(&RTPManager::OnRTCPData, this, std::placeholders::_1, std::placeholders::_2);
	m_rtcpServer.start();
}

void RTPManager::Stop()
{
	m_rtpServer.stop();

	if (m_rtpPacketHandler) {
		m_rtpPacketHandler = nullptr;
	}
}

void RTPManager::SetSourceIP(std::string& sourceIP)
{
	m_sourceIP = sourceIP;
}

uint16_t RTPManager::GetRTPPort()
{
	return m_rtpPort;
}

void RTPManager::SetRTCPServerPort(uint16_t port)
{
	m_rtcpServerPort = port;
}

void RTPManager::SetMiracastCallback(std::shared_ptr<IMiracastCallback> callback)
{
	m_miracastCallback = callback;
}

void RTPManager::SetStreamID(uint32_t streamId)
{
	m_streamId = streamId;
}

void RTPManager::OnData(int type, uint8_t* buffer, int bufSize)
{
#if defined(HHDUMP_TEST)
#if defined(ANDROID)
	// dump h264 file
	if (fH264 != nullptr && type == 1)
		fwrite((uint8_t*)buffer, 1, bufSize, fH264);
#endif
#endif

	if (m_miracastCallback) {
		m_miracastCallback->OnData(m_streamId, type, buffer, bufSize);
	}
}

void RTPManager::InputTSData(const uint8_t* data, size_t bytes)
{
#if defined(HHDUMP_TEST)
#if defined(ANDROID)
	// dump TS file
	if (fTS != nullptr)
		fwrite((uint8_t*)data, 1, bytes, fTS);
#endif
#endif

	m_tsDecoder->input(data, bytes);
}

bool RTPManager::GenerateRtpPort(uint16_t& port)
{
	std::random_device rd;
	for (int n = 0; n <= 10; n++)
	{
		if (n == 10)
			return false;

		port = rd() & 0xfffe;

		SOCKET _rtpfd = ::socket(AF_INET, SOCK_DGRAM, 0);
		struct sockaddr_in addr = { 0 };
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = inet_addr("0.0.0.0");
		addr.sin_port = htons(port);

		if (::bind(_rtpfd, (struct sockaddr*)&addr, sizeof addr) < 0)
		{
			::closesocket(_rtpfd);
			continue;
		}

		::closesocket(_rtpfd);
		break;
	}

	return true;
}

void RTPManager::OnPayloadData(const hv::SocketChannelPtr& channel, hv::Buffer* msg)
{
	if (m_mediaSSRC == 0) {
		m_mediaSSRC = (((uint32_t)((uint8_t*)msg->data())[8]) << 24) | (((uint32_t)((uint8_t*)msg->data())[9]) << 16) | (((uint32_t)((uint8_t*)msg->data())[10]) << 8) | ((uint8_t*)msg->data())[11];
	}

    m_rtpPacketHandler->addPayload((uint8_t*)msg->data(), msg->size());
}

void RTPManager::OnRTCPData(const hv::SocketChannelPtr& channel, hv::Buffer* msg)
{
	//OnRTCPData Not Used, Reserved

	//DebugL << "Recv RTCPData: " << msg->data() << std::endl;
}

void RTPManager::OnPacketLoss(uint16_t seqNumber)
{
	//std::cout << "RTPManager::onPacketLoss seqNumber: " << seqNumber << std::endl;
}