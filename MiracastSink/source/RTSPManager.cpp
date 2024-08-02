#include "pch.h"
#include "RTSPManager.h"
#include <regex>

static unsigned int IPToStreamID(const std::string& ipAddress) {
	struct in_addr addr;

	// 将 IP 地址字符串转换为网络字节序的二进制格式
	if (inet_pton(AF_INET, ipAddress.c_str(), &addr) == 1) {
		// 将网络字节序的地址转换为主机字节序的无符号整数
		return ntohl(addr.s_addr);
	}
	else {
		std::cerr << "Invalid IP address: " << ipAddress << std::endl;
		return 0;
	}
}

RTSPManager::RTSPManager(): m_userAgent("HHServer"), m_start(false)
{
	m_rtpManager = std::make_shared<RTPManager>();
}

RTSPManager::~RTSPManager()
{

}

void RTSPManager::Start(const std::string& ip)
{
	{
		std::lock_guard<std::mutex> lg(m_mtxStop);

		if (m_start)
			return;

		m_start = true;
	}

	m_sourceIP = ip;

	m_rtspClient = std::make_shared<hv::TcpClient>();
	m_rtspClient->createsocket(7236, m_sourceIP.c_str());
	m_rtspClient->onConnection = std::bind(&RTSPManager::OnConnection, this, std::placeholders::_1);
	m_rtspClient->onMessage = std::bind(&RTSPManager::OnMessage, this, std::placeholders::_1, std::placeholders::_2);
	m_rtspClient->start();

	std::cout << "Start RTSP Connect " << m_sourceIP + ":7236" << std::endl;
}

void RTSPManager::Stop()
{
	{
		std::lock_guard<std::mutex> lg(m_mtxStop);

		if (!m_start)
			return;

		m_start = false;
	}

	// send TearDown
	std::shared_ptr<char> req(new char[2048]);
	int size = 0;
	size = BuildTearDown(req.get(), 2048);
	SendMsg(req, size);

	if (m_rtspClient && m_rtspClient->isConnected())
	{
		m_rtspClient->stop();
	}

	m_rtpManager->Stop();
}

void RTSPManager::SetDeviceName(std::string name)
{
	m_deviceName = name;
}

void RTSPManager::SetStreamID(uint32_t streamId)
{
	m_streamId = streamId;

	m_rtpManager->SetStreamID(streamId);
}

void RTSPManager::SetMiracastCallback(std::shared_ptr<IMiracastCallback> callback)
{
	m_miracastCallback = callback;
	m_rtpManager->SetMiracastCallback(callback);
}

void RTSPManager::SendHIDMouse(unsigned char type, char xdiff, char ydiff)
{
	m_uibcManager->SendHIDMouse(type, xdiff, ydiff);
}

void RTSPManager::SendHIDKeyboard(unsigned char type, unsigned char modType, unsigned short keyboardValue)
{
	m_uibcManager->SendHIDKeyboard(type, modType, keyboardValue);
}

void RTSPManager::OnConnection(const hv::SocketChannelPtr& conn)
{
	std::string sourceIP = conn->peeraddr();
	if (conn->isConnected())
	{
		std::cout << "MiracastSink RTSP onConnection: " << m_deviceName << std::endl;
		if (m_miracastCallback)
			m_miracastCallback->OnConnect(m_streamId);
	}
	else
	{
		std::cout << "MiracastSink RTSP onDisconnected: " << m_deviceName << std::endl;
		if (m_miracastCallback)
			m_miracastCallback->OnDisconnect(m_streamId);
	}
}

void RTSPManager::OnMessage(const hv::SocketChannelPtr& channel, hv::Buffer* msg)
{
	std::string s((char*)msg->data(), msg->size());

	std::cout << "RTSP OnMessage: " << std::endl;
	std::cout << s << std::endl;

	const char* buf = s.c_str();
	ParseResponse(buf);

	Method method = GetMethod();
	switch (method)
	{
	case OPTIONS:
		HandleOptions();
		break;
	case GET_PARAMETER:
		HandleGetParamter();
		break;
	case SET_PARAMETER:
		HandleSetParamter();
		break;
	case SETUP:
		// M5
		HandleSetup();
		break;
	case PLAY:
		// M7
		HandlePlay();
		break;
	case TEARDOWN:
		HandleTearDown();
		break;
	default:
		break;
	}
}

void RTSPManager::ParseResponse(const char* buffer)
{
	std::string message(buffer);
	if (message.find("RTSP/1.0 200 OK") != std::string::npos)
	{
		ParseSessionID(message);
		if (message.find("GET_PARAMETER rtsp://localhost/wfd1.0 RTSP/1.0") != std::string::npos)
			m_method = GET_PARAMETER;

		return;
	}

	ParseRequestLine(buffer);
	ParseCSeq(message);
}

bool RTSPManager::ParseRequestLine(const char* msg)
{
	char method[64] = { 0 };
	char url[512] = { 0 };
	char version[64] = { 0 };

	if (sscanf(msg, "%s %s %s", method, url, version) != 3)
	{
		return false;
	}

	std::string methodString(method);

	if (methodString == "OPTIONS")
	{
		m_method = OPTIONS;
	}
	else if (methodString == "ANNOUNCE")
	{
		m_method = ANNOUNCE;
	}
	else if (methodString == "RECORD")
	{
		m_method = RECORD;
	}
	else if (methodString == "DESCRIBE")
	{
		m_method = DESCRIBE;
	}
	else if (methodString == "SETUP")
	{
		m_method = SETUP;
	}
	else if (methodString == "PLAY")
	{
		m_method = PLAY;
	}
	else if (methodString == "TEARDOWN")
	{
		m_method = TEARDOWN;
	}
	else if (methodString == "GET_PARAMETER")
	{
		m_method = GET_PARAMETER;
	}
	else if (methodString == "SET_PARAMETER")
	{
		std::string temp = std::string(msg);
		ParseSetParameter(temp);
	}
	else
	{
		m_method = NONE;
		return false;
	}

	if (strncmp(url, "rtsp://", 7) != 0)
	{
		return false;
	}

	return true;
}

bool RTSPManager::ParseSetParameter(std::string& msg)
{
	std::size_t pos = msg.find("wfd_presentation_URL");
	if (pos != std::string::npos)
	{
		char url[256] = { 0 };
		sscanf(msg.c_str() + pos, "wfd_presentation_URL: %s none", url);
		m_rtspUrl = url;
		m_method = SET_PARAMETER;
	}
	else if (msg.find("wfd_trigger_method: SETUP") != std::string::npos)
	{
		// M5
		m_method = SETUP;
	}
	else if (msg.find("wfd_trigger_method: TEARDOWN") != std::string::npos)
	{
		m_method = TEARDOWN;
	}

	if (msg.find("wfd_uibc_capability") != std::string::npos)
	{
		m_uibcPort = GetUIBCPort(msg);
		std::cout << "UIBC Port: " << m_uibcPort << std::endl;
	}

	return true;
}

bool RTSPManager::ParseSessionID(std::string& message)
{
	std::size_t pos = message.find("Session");
	if (pos != std::string::npos)
	{
		if (sscanf(message.c_str() + pos, "%*[^:]: %u", &m_sessionID) != 1)
			return false;

		return true;
	}

	return false;
}

bool RTSPManager::ParseCSeq(std::string& message)
{
	std::size_t pos = message.find("CSeq");
	if (pos != std::string::npos)
	{
		uint32_t cseq = 0;
		sscanf(message.c_str() + pos, "%*[^:]: %u", &cseq);
		m_cseq = cseq;

		return true;
	}

	return false;
}

void RTSPManager::HandleOptions()
{
	std::shared_ptr<char> res(new char[2048]);
	// M1
	int size = BuildOptionResponse(res.get(), 2048);
	SendMsg(res, size);
	// M2
	size = BuildOptionRequest(res.get(), 2048);
	SendMsg(res, size);
}

void RTSPManager::HandleGetParamter()
{
	std::shared_ptr<char> res(new char[2048]);
	int size;
	if (!m_initRtpServer)
	{
		SetupRtpServer();
		size = BuildGetParamterResponseM3(res.get(), 2048, m_rtpManager->GetRTPPort());
		SendMsg(res, size);
		m_initRtpServer = true;
	}
	else
	{
		size = BuildGetParamterResponseM16(res.get(), 2048);
		SendMsg(res, size);
	}
}

void RTSPManager::HandleSetParamter()
{
	std::shared_ptr<char> res(new char[2048]);
	int size;
	// M4
	size = BuildSetParamterResponse(res.get(), 2048, 0);
	SendMsg(res, size);

	if (m_uibcManager == nullptr)
	{
		m_uibcManager = std::make_shared<UIBCManager>();
		m_uibcManager->ConnectUIBC(m_sourceIP, m_uibcPort);
	}
}

void RTSPManager::HandleSetup()
{
	std::shared_ptr<char> res(new char[2048]);
	int size;

	size = BuildSetParamterResponse(res.get(), 2048, 0);
	SendMsg(res, size);

	// M6
	size = BuildSetupRequest(res.get(), 2048, m_rtpManager->GetRTPPort());
	SendMsg(res, size);
}

void RTSPManager::HandlePlay()
{
	std::shared_ptr<char> req(new char[2048]);
	int size = 0;
	size = BuildPlayRequest(req.get(), 2048);
	SendMsg(req, size);
	size = BuildIdrRequest(req.get(), 2048);
	SendMsg(req, size);

	// notify launch player
	if (m_miracastCallback){
		m_miracastCallback->OnPlay(m_streamId);
	}
}

void RTSPManager::HandleTearDown()
{
	Stop();
}

void RTSPManager::RequestIdr()
{
	std::shared_ptr<char> req(new char[2048]);
	int size = 0;
	size = BuildIdrRequest(req.get(), 2048);

	SendMsg(req, size);
}

int RTSPManager::BuildOptionResponse(const char* buf, int bufSize)
{
	memset((void*)buf, 0, bufSize);
	snprintf((char*)buf, bufSize,
		"RTSP/1.0 200 OK\r\n"
		"CSeq: %u\r\n"
		"Public: org.wfa.wfd1.0, GET_PARAMETER, SET_PARAMETER\r\n"
		"\r\n",
		GetCSeq());

	return strlen(buf);
}

int RTSPManager::BuildOptionRequest(const char* buf, int bufSize)
{
	memset((void*)buf, 0, bufSize);
	snprintf((char*)buf, bufSize,
		"OPTIONS * RTSP/1.0\r\n"
		"CSeq: %u\r\n"
		"Require: org.wfa.wfd1.0\r\n"
		"\r\n",
		m_cseq++);

	m_method = NONE;
	return strlen(buf);
}

int RTSPManager::BuildGetParamterResponseM3(const char* buf, int bufSize, uint32_t port)
{
	const char* paramter = "wfd_client_rtp_ports: RTP/AVP/UDP;unicast %d 0 mode=play\r\n"
		"wfd_audio_codecs: LPCM 00000003 00, AAC 0000000f 00, AC3 00000007 00\r\n"
		"wfd_video_formats: 40 00 01 10 0001bdeb 051557ff 00003fff 10 0000 001f 11 0780 0438, 02 10 0001bdeb 155557ff 00000fff 10 0000 001f 11 0780 0438\r\n"
		"wfd_content_protection: none\r\n"
		"wfd_uibc_capability: input_category_list=HIDC; hidc_cap_list=Keyboard/USB, Mouse/USB, MultiTouch/USB, Gesture/USB, RemoteControl/USB, Joystick/USB; port=none\r\n\r\n";
	//"wfd_uibc_capability: input_category_list=GENERIC; generic_cap_list=Keyboard, Mouse; port = none\r\n\r\n";
	//"wfd_uibc_capability: input_category_list=GENERIC, HIDC; generic_cap_list=Keyboard, Mouse, MultiTouch, Gesture, RemoteControl, Joystick; hidc_cap_list=Keyboard/USB, Mouse/USB, MultiTouch/USB, Gesture/USB, RemoteControl/USB, Joystick/USB; port = none\r\n\r\n";

	char paramters[1500] = { 0 };
	sprintf(paramters, paramter, port);

	int len = strlen(paramters);
	memset((void*)buf, 0, bufSize);
	snprintf((char*)buf, bufSize,
		"RTSP/1.0 200 OK\r\n"
		"Content-Length: %d\r\n"
		"Content-Type: text/parameters\r\n"
		"CSeq: %d\r\n"
		"\r\n"
		"%s",
		len,
		GetCSeq(),
		paramters);

	m_method = NONE;
	return strlen(buf);
}

int RTSPManager::BuildGetParamterResponseM16(const char* buf, int bufSize)
{
	memset((void*)buf, 0, bufSize);
	snprintf((char*)buf, bufSize,
		"RTSP/1.0 200 OK\r\n"
		"CSeq: %d\r\n"
		"\r\n",
		m_cseq);

	return strlen(buf);
}

int RTSPManager::BuildSetParamterResponse(const char* buf, int bufSize, int seq)
{
	memset((void*)buf, 0, bufSize);
	snprintf((char*)buf, bufSize,
		"RTSP/1.0 200 OK\r\n"
		"CSeq: %d\r\n"
		"\r\n",
		m_cseq++);

	return strlen(buf);
}

int RTSPManager::BuildSetupRequest(const char* buf, int bufSize, uint16_t port)
{
	memset((void*)buf, 0, bufSize);
	snprintf((char*)buf, bufSize,
		"SETUP %s RTSP/1.0\r\n"
		"Transport: RTP/AVP/UDP;unicast;client_port=%d;mode=play\r\n"
		"CSeq: %u\r\n"
		"\r\n",
		m_rtspUrl.c_str(),
		port,
		m_cseq++);

	m_method = PLAY;
	return strlen(buf);
}

int RTSPManager::BuildPlayRequest(const char* buf, int bufSize)
{
	memset((void*)buf, 0, bufSize);
	snprintf((char*)buf, bufSize,
		"PLAY %s RTSP/1.0\r\n"
		"CSeq: %u\r\n"
		"Session: %d\r\n"
		"\r\n",
		m_rtspUrl.c_str(),
		m_cseq++,
		m_sessionID);

	m_method = NONE;
	return strlen(buf);
}

int RTSPManager::BuildTearDown(const char* buf, int bufSize)
{
	memset((void*)buf, 0, bufSize);
	snprintf((char*)buf, bufSize,
		"TEARDOWN %s RTSP/1.0\r\n"
		"CSeq: %u\r\n"
		"User-Agent: %s\r\n"
		"Session: %d\r\n"
		"\r\n",
		m_rtspUrl.c_str(),
		m_cseq++,
		m_userAgent.c_str(),
		m_sessionID);

	m_method = NONE;
	return strlen(buf);
}

int RTSPManager::BuildIdrRequest(const char* buf, int bufSize)
{
	memset((void*)buf, 0, bufSize);
	snprintf((char*)buf, bufSize,
		"SET_PARAMETER %s RTSP/1.0\r\n"
		"CSeq: %u\r\n"
		"Session: %d\r\n"
		"Content-Type: text/parameters\r\n"
		"Content-Length: 17\r\n"
		"\r\n"
		"wfd_idr_request\r\n",
		m_rtspUrl.c_str(),
		m_cseq++,
		m_sessionID);

	return strlen(buf);
}

void RTSPManager::SendMsg(std::shared_ptr<char> buf, uint32_t size)
{
	if (m_rtspClient->isConnected()) {
		m_rtspClient->send(buf.get(), size);
		std::cout << "RTSP SendMessage: " << std::endl;
		std::cout << buf << std::endl;
	}
}

void RTSPManager::SetupRtpServer()
{
	m_rtpManager->Start();
}

int RTSPManager::GetUIBCPort(const std::string& input)
{
	std::regex regexPattern(R"(port=(\d+))");
	std::smatch match;
	if (std::regex_search(input, match, regexPattern)) {
		if (match.size() > 1) {
			return std::stoi(match.str(1));
		}
	}
	else {
		std::cout << "Port not found in the input string." << std::endl;
		return 0;
	}
}

RTSPManager::Method RTSPManager::GetMethod()
{
	return m_method;
}

uint32_t RTSPManager::GetCSeq()
{
	return m_cseq;
}
