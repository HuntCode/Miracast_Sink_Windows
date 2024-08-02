#include "pch.h"
#include <iostream>
#include <random>

#include "RTPManager.h"
#include "TSDemux.h"

#define TS_PACKET_LEN 188

TSParseWorker::TSParseWorker(RTPManager* owner) :
	m_owner(owner), m_pTSDemux(nullptr)
{

}

void TSParseWorker::ThreadPrepare()
{
#ifdef WIN32
	fH264 = fopen("D:\\A_ReservedLand\\output.h264", "wb+");
	fAAC = fopen("D:\\A_ReservedLand\\output.aac", "wb+");
#endif
	std::cout << "TSParseWorker Start... id: " << std::this_thread::get_id() << std::endl;
}

void TSParseWorker::ThreadProcess()
{
	if (m_pTSDemux == nullptr) {
		m_pTSDemux = TSDemuxCreate();//MPEGTS´¦Àí
	}
	uint8_t tsData[TS_PACKET_LEN];
	if (m_owner->TSDataBuffer().Size() >= TS_PACKET_LEN) {
		m_owner->TSDataBuffer().Read(tsData, TS_PACKET_LEN);
		int ret = TSDemuxWriteData(m_pTSDemux, tsData, TS_PACKET_LEN);
		if (ret == TS_TYPE_AUDIO) {
			uint8_t* pAudio = nullptr;
			int nAudioSize = 0;
			TSDemuxGetAudioData(m_pTSDemux, &pAudio, &nAudioSize);
#ifdef _DEBUG
			//std::cout << "nAudioSize = " << nAudioSize << std::endl;
#endif
			//fwrite(pAudio, 1, nAudioSize, fAAC);
			m_owner->OnData(TS_TYPE_AUDIO, pAudio, nAudioSize);
		}
		else if (ret == TS_TYPE_VIDEO) {
			uint8_t* pVideo = nullptr;
			int nVideoSize = 0;
			TSDemuxGetVideoData(m_pTSDemux, &pVideo, &nVideoSize);
#ifdef _DEBUG
			//std::cout << "nVideoSize =" << nVideoSize << std::endl;
#endif
			//fwrite(pVideo, 1, nVideoSize, fH264);

			m_owner->OnData(TS_TYPE_VIDEO, pVideo, nVideoSize);
		}
	}
	else {
#ifdef WIN32
		Sleep(1);
#else
		usleep(1);
#endif
	}
}

void TSParseWorker::ThreadPost()
{
	if (m_pTSDemux) {

		TSDemuxDestroy(m_pTSDemux);
		m_pTSDemux = nullptr;
	}

	std::cout << "TSParseWorker Stop..." << std::endl;
}

RTPManager::RTPManager():
	m_tsDataBuffer(1920 * 1080 * 200),
	m_tsParseWorker(nullptr)
{

}

RTPManager::~RTPManager()
{

}

void RTPManager::Start()
{
	if (m_tsParseWorker == nullptr) {
		m_tsParseWorker = std::make_unique<TSParseWorker>(this);
		m_tsParseWorker->ThreadStart();
	}

	GenerateRtpPort(m_rtpPort);
	m_rtpServer.createsocket(m_rtpPort);
	m_rtpServer.onMessage = std::bind(&RTPManager::OnPayloadData, this, std::placeholders::_1, std::placeholders::_2);
	m_rtpServer.start();

}

void RTPManager::Stop()
{
	m_rtpServer.stop();

	if (m_tsParseWorker) {
		m_tsParseWorker->ThreadStop();
		m_tsParseWorker.reset();
	}
}

uint16_t RTPManager::GetRTPPort()
{
	return m_rtpPort;
}

WXFifo& RTPManager::TSDataBuffer()
{
	return m_tsDataBuffer;
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
	if (m_miracastCallback) {
		m_miracastCallback->OnData(m_streamId, type, buffer, bufSize);
	}
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

		if (::bind(_rtpfd, (struct sockaddr*)&addr, sizeof addr) == SOCKET_ERROR)
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
	std::string s((char*)msg->data(), msg->size());
	if (s.length() > 12)
	{
		m_tsDataBuffer.Write((uint8_t*)(s.c_str() + 12), s.length() - 12);
	}
}
