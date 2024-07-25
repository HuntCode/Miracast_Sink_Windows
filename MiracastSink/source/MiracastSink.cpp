#include "pch.h"
#include <random>

#include "MiracastSink.h"
#include "TSDemux.h"
#include "MiracastWorker.h"

#define TS_PACKET_LEN 188

//extern std::shared_ptr<MiracastWorker> g_miracastWorker;

TSParseWorker::TSParseWorker(MiracastSink* owner) :
	m_owner(owner), m_pTSDemux(nullptr)
{

}

void TSParseWorker::ThreadPrepare()
{
	fH264 = fopen("D:\\A_ReservedLand\\output.h264", "wb+");
	fAAC = fopen("D:\\A_ReservedLand\\output.aac", "wb+");
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
		usleep(1);
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

MiracastSink::MiracastSink() :
	m_conn(nullptr),
	_rtspResponsePtr(new RtspResponse),
	m_uibcManager(nullptr),
	m_tsDataBuffer(1920 * 1080 * 200),
	m_pTSDemux(nullptr)
{
	//m_loop = loop;
	m_eventLoopManager.Start();
	_rtspResponsePtr->setUserAgent("HHServer");
}

MiracastSink::~MiracastSink()
{
	close();
}

bool MiracastSink::Start(std::string ip)
{
	m_remoteIP = ip;
	string addr = m_remoteIP + ":7236";

	_tcpClient = std::shared_ptr<evpp::TCPClient>(new evpp::TCPClient(m_eventLoopManager.GetLoop(), addr, "Miracast"));
	_tcpClient->SetMessageCallback(std::bind(&MiracastSink::onMessage, this, std::placeholders::_1, std::placeholders::_2));
	_tcpClient->SetConnectionCallback(std::bind(&MiracastSink::onConnection, this, std::placeholders::_1));
	_tcpClient->set_connecting_timeout(evpp::Duration(1.0));
	_tcpClient->set_auto_reconnect(false);
	_tcpClient->Connect();

	std::cout << "Start RTSP Connect: " << addr << std::endl;

	return true;
}

void MiracastSink::SetMiracastConn(MiracastReceiverConnection conn)
{
	m_conn = conn;
}

void MiracastSink::SetDeviceName(std::string name)
{
	m_deviceName = name;
}

WXFifo& MiracastSink::TSDataBuffer()
{
	return m_tsDataBuffer;
}

void MiracastSink::OnData(int type, uint8_t* buffer, int bufSize)
{
	if (type == TS_TYPE_VIDEO) {
		m_sdlPlayer->ProcessVideo(buffer, bufSize);
	}
	else if (type == TS_TYPE_AUDIO) {
		m_sdlPlayer->ProcessAudio(buffer, bufSize);
	}
}

std::shared_ptr<UIBCManager>& MiracastSink::GetUIBCManager()
{
	return m_uibcManager;
}

void MiracastSink::close()
{
	if (m_tsParseWorker)
	{
		m_tsParseWorker->ThreadStop();
	}

	_serverStream.Stop(true);

	if (_isConnected)
	{
		_tcpClient->Disconnect();
		//_tcpClient->Close();

		_clientUdp.Close();
	}
	else
	{
		_reConnNums = 100;
		std::this_thread::sleep_for(chrono::milliseconds(1200));
	}
}

void MiracastSink::onMessage(const evpp::TCPConnPtr& conn, evpp::Buffer* msg)
{
	string s = msg->NextAllString();

	std::cout << "RTSP OnMessage: " << std::endl;
	std::cout << s << std::endl;

	const char* buf = s.c_str();
	if (_rtspResponsePtr->parseResponse(buf))
	{
		RtspResponse::Method method = _rtspResponsePtr->getMethod();
		switch (method)
		{
		case RtspResponse::OPTIONS:
			handleCmdOption();
			break;
		case RtspResponse::GET_PARAMETER:
			handleCmdGetParamter();
			break;
		case RtspResponse::SET_PARAMETER:
			handleCmdSetParamter();
			break;
		case RtspResponse::SETUP:
			handleSetup();
			break;
		case RtspResponse::PLAY:
			handlePlay();
			break;
		case RtspResponse::TEARDOWN:
			handleTearDown();
			break;
		default:
			break;
		}
	}
}

void MiracastSink::onConnection(const evpp::TCPConnPtr& conn)
{
	if (conn->IsConnected())
	{
		std::cout << "MiracastSink RTSP onConnection: " << m_deviceName << std::endl;
		_isConnected = true;
	}
	else
	{
		std::cout << "MiracastSink RTSP onDisconnected: " << m_deviceName << std::endl;
		if (_isConnected)
		{
			_isConnected = false;
#ifdef _DEBUG
			_tcpClient->Close();
#endif // DEBUG
			m_conn.Close();
		}
		else
		{
			if (_reConnNums < 15)
				_tcpClient->Connect();
			else
			{
#ifdef _DEBUG
				_tcpClient->Close();
#endif // DEBUG
				std::cout << " connect " << m_deviceName << " 7236 error" << std::endl;
			}
			_reConnNums++;
		}
	}
}

void MiracastSink::sendMessage(std::shared_ptr<char> buf, uint32_t size)
{
	if (_isConnected) {
		_tcpClient->conn()->Send(buf.get(), size);
		std::cout << "RTSP SendMessage: " << std::endl;
		std::cout << buf << std::endl;
	}
}

void MiracastSink::onPayloadData(evpp::EventLoop* loop, evpp::udp::MessagePtr& msg)
{
	std::string s = msg->NextAllString();
	if (s.length() > 12)
	{
		m_tsDataBuffer.Write((uint8_t*)(s.c_str() + 12), s.length() - 12);
	}
}

void MiracastSink::handleCmdOption()
{
	std::shared_ptr<char> res(new char[2048]);
	int size = _rtspResponsePtr->buildOptionRes(res.get(), 2048);
	sendMessage(res, size);

	size = _rtspResponsePtr->buildOptionReq(res.get(), 2048);
	sendMessage(res, size);
}

void MiracastSink::handleCmdGetParamter()
{
	std::shared_ptr<char> res(new char[2048]);
	int size;
	if (_initRtspUdp)
	{
		size = _rtspResponsePtr->buildGetParamterResEx(res.get(), 2048);
		sendMessage(res, size);
	}
	else
	{
		setupRtpOverUdp();
		size = _rtspResponsePtr->buildGetParamterRes(res.get(), 2048, _server_port);
		sendMessage(res, size);
		_initRtspUdp = true;
	}
}

void MiracastSink::handleCmdSetParamter()
{
	std::shared_ptr<char> res(new char[2048]);
	int size;

	size = _rtspResponsePtr->buildSetParamterRes(res.get(), 2048, 0);
	sendMessage(res, size);

	if (m_uibcManager == nullptr)
	{
		m_uibcManager = std::make_shared<UIBCManager>();
		m_uibcManager->ConnectUIBC(m_remoteIP, _rtspResponsePtr->GetUIBCPort());
	}
}

void MiracastSink::handleSetup()
{
	std::shared_ptr<char> res(new char[2048]);
	int size;

	size = _rtspResponsePtr->buildSetParamterRes(res.get(), 2048, 0);
	sendMessage(res, size);

	size = _rtspResponsePtr->buildSetupUdpReq(res.get(), 2048, _server_port);
	sendMessage(res, size);
}

void MiracastSink::handlePlay()
{
	std::shared_ptr<char> req(new char[2048]);
	int size = 0;
	size = _rtspResponsePtr->buildPlayReq(req.get(), 2048);
	sendMessage(req, size);
	size = _rtspResponsePtr->buildIdrReq(req.get(), 2048);
	sendMessage(req, size);
	
	std::thread([this]() {
		if(m_sdlPlayer==nullptr)
			m_sdlPlayer = std::make_shared<SDLPlayer>(1920, 1080, this);
		m_sdlPlayer->Init("SDLPlayer");
		m_sdlPlayer->Play();

		//g_miracastWorker.RunTask([]() {
			MiracastApp::instance()->StopMiracast();
		//	});

		}).detach();

	m_tsParseWorker = std::make_unique<TSParseWorker>(this);
	m_tsParseWorker->ThreadStart();
}

void MiracastSink::handleTearDown()
{
	//std::shared_ptr<char> req(new char[2048]);
	//int size = 0;
	//size = _rtspResponsePtr->buildTeardown(req.get(), 2048);
	//sendMessage(req, size);

	m_tsParseWorker->ThreadStop();
	m_tsParseWorker.reset();
	m_sdlPlayer.reset();
}

bool MiracastSink::setupRtpOverUdp()
{
	_serverStream.SetThreadDispatchPolicy(evpp::ThreadDispatchPolicy::kIPAddressHashing);
	_serverStream.SetMessageHandler(std::bind(&MiracastSink::onPayloadData, this, std::placeholders::_1, std::placeholders::_2));
	_serverStream.set_recv_buf_size(65535);
	//_serverStream.set_recv_buf_size(1024 * 500);
	generateRtpPort(_server_port);
	_serverStream.Init(_server_port);
	_serverStream.Start();

	static int port = 6000;
	_clientUdp.Connect("127.0.0.1", port);
	char url[64] = { 0 };
	sprintf(url, "rtp://127.0.0.1:%d", port++);

	return true;

}

void MiracastSink::requestIdr()
{
	std::shared_ptr<char> req(new char[2048]);
	int size = 0;
	size = _rtspResponsePtr->buildIdrReq(req.get(), 2048);

	sendMessage(req, size);
}

bool MiracastSink::generateRtpPort(uint16_t& port)
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
