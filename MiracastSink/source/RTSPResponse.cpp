#include "pch.h"
#include "RtspResponse.h"
#include <regex>


RtspResponse::RtspResponse():m_uibcPort(0)
{

}

RtspResponse::~RtspResponse()
{

}

bool RtspResponse::parseSetParameter(std::string& message)
{
	std::size_t pos = message.find("wfd_presentation_URL");
	if (pos != std::string::npos)
	{
		char url[256] = { 0 };
		sscanf(message.c_str() + pos, "wfd_presentation_URL: %s none", url);
		_rtspUrl = url;
		_method = SET_PARAMETER;
	}
	else if (message.find("wfd_trigger_method: SETUP") != std::string::npos)
	{
		_method = SETUP;
	}
	else if (message.find("wfd_trigger_method: TEARDOWN") != std::string::npos)
	{
		_method = TEARDOWN;
	}
	
	if (message.find("wfd_uibc_capability") != std::string::npos)
	{
		m_uibcPort = extractPort(message);
		std::cout << "UIBC Port: " << m_uibcPort << std::endl;
	}

	return true;
}

bool RtspResponse::parseRequestLine(const char* message)
{
	char method[64] = { 0 };
	char url[512] = { 0 };
	char version[64] = { 0 };

	if (sscanf(message, "%s %s %s", method, url, version) != 3)
	{
		return false;
	}

	string methodString(method);

	if (methodString == "OPTIONS")
	{
		_method = OPTIONS;
	}
	else if (methodString == "ANNOUNCE")
	{
		_method = ANNOUNCE;
	}
	else if (methodString == "RECORD")
	{
		_method = RECORD;
	}
	else if (methodString == "DESCRIBE")
	{
		_method = DESCRIBE;
	}
	else if (methodString == "SETUP")
	{
		_method = SETUP;
	}
	else if (methodString == "PLAY")
	{
		_method = PLAY;
	}
	else if (methodString == "TEARDOWN")
	{
		_method = TEARDOWN;
	}
	else if (methodString == "GET_PARAMETER")
	{
		_method = GET_PARAMETER;
	}
	else if (methodString == "SET_PARAMETER")
	{
		std::string temp = std::string(message);
		parseSetParameter(temp);
	}
	else
	{
		_method = NONE;
		return false;
	}

	if (strncmp(url, "rtsp://", 7) != 0)
	{
		return false;
	}

	return true;
}

bool RtspResponse::parseSessionId(std::string& message)
{
	std::size_t pos = message.find("Session");
	if (pos != std::string::npos)
	{
		if (sscanf(message.c_str() + pos, "%*[^:]: %u", &_sessionId) != 1)
			return false;

		return true;
	}

	return false;
}

bool RtspResponse::parseCSeq(std::string& message)
{
	std::size_t pos = message.find("CSeq");
	if (pos != std::string::npos)
	{
		uint32_t cseq = 0;
		sscanf(message.c_str() + pos, "%*[^:]: %u", &cseq);
		_cseq_res = cseq;

		return true;
	}

	return false;
}

bool RtspResponse::parseResponse(const char* buffer)
{
	string message(buffer);
	if (message.find("RTSP/1.0 200 OK") != string::npos)
	{
		parseSessionId(message);
		if (message.find("GET_PARAMETER rtsp://localhost/wfd1.0 RTSP/1.0") != string::npos)
			_method = GET_PARAMETER;

		return true;
	}

	parseRequestLine(buffer);
	parseCSeq(message);

	return true;
}

int RtspResponse::buildSetParamterRes(const char* buf, int bufSize, int seq)
{
	memset((void*)buf, 0, bufSize);
	snprintf((char*)buf, bufSize,
		"RTSP/1.0 200 OK\r\n"
		"CSeq: %d\r\n"
		"\r\n",
		_cseq_res);

	return strlen(buf);
}

int RtspResponse::buildGetParamterResEx(const char* buf, int bufSize)
{
	memset((void*)buf, 0, bufSize);
	snprintf((char*)buf, bufSize,
		"RTSP/1.0 200 OK\r\n"
		"CSeq: %d\r\n"
		"\r\n",
		_cseq_res);

	return strlen(buf);
}

int RtspResponse::buildGetParamterRes(const char* buf, int bufSize, uint32_t port)
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
		this->getCSeq(),
		paramters);

	_method = NONE;
	return strlen(buf);
}

int RtspResponse::buildOptionRes(const char* buf, int bufSize)
{
	memset((void*)buf, 0, bufSize);
	snprintf((char*)buf, bufSize,
		"RTSP/1.0 200 OK\r\n"
		"CSeq: %u\r\n"
		"Public: org.wfa.wfd1.0, GET_PARAMETER, SET_PARAMETER\r\n"
		"\r\n",
		this->getCSeq());

	return strlen(buf);
}

int RtspResponse::buildOptionReq(const char* buf, int bufSize)
{
	memset((void*)buf, 0, bufSize);
	snprintf((char*)buf, bufSize,
		"OPTIONS * RTSP/1.0\r\n"
		"CSeq: %u\r\n"
		"Require: org.wfa.wfd1.0\r\n"
		"\r\n",
		_cseq++);

	_method = NONE;
	return strlen(buf);
}

int RtspResponse::buildSetupUdpReq(const char* buf, int bufSize, uint16_t port)
{
	memset((void*)buf, 0, bufSize);
	snprintf((char*)buf, bufSize,
		"SETUP %s RTSP/1.0\r\n"
		"Transport: RTP/AVP/UDP;unicast;client_port=%d;mode=play\r\n"
		"CSeq: %u\r\n"
		"\r\n",
		_rtspUrl.c_str(),
		port,
		_cseq++);

	_method = PLAY;
	return strlen(buf);
}

int RtspResponse::buildIdrReq(const char* buf, int bufSize)
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
		_rtspUrl.c_str(),
		_cseq++,
		_sessionId);

	return strlen(buf);
}

int RtspResponse::buildPlayReq(const char* buf, int bufSize)
{
	memset((void*)buf, 0, bufSize);
	snprintf((char*)buf, bufSize,
		"PLAY %s RTSP/1.0\r\n"
		"CSeq: %u\r\n"
		"Session: %d\r\n"
		"\r\n",
		_rtspUrl.c_str(),
		_cseq++,
		getSession());

	_method = NONE;
	return strlen(buf);
}

int RtspResponse::buildTeardown(const char* buf, int bufSize)
{
	memset((void*)buf, 0, bufSize);
	snprintf((char*)buf, bufSize,
		"TEARDOWN %s RTSP/1.0\r\n"
		"CSeq: %u\r\n"
		"User-Agent: %s\r\n"
		"Session: %d\r\n"
		"\r\n",
		_rtspUrl.c_str(),
		_cseq++,
		_userAgent.c_str(),
		_sessionId);

	_method = NONE;
	return strlen(buf);
}

int RtspResponse::extractPort(const std::string& input)
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