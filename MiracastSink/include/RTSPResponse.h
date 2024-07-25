#ifndef _RTSP_RESPONSE_H
#define _RTSP_RESPONSE_H

#include <iostream>
#include <string>
#include <cstring>

using namespace std;

class RtspResponse
{
public:
	RtspResponse();
	~RtspResponse();
	enum Method
	{
		OPTIONS,
		DESCRIBE,
		ANNOUNCE,
		SETUP,
		RECORD,
		PLAY,
		GET_PARAMETER,
		TEARDOWN,
		SET_PARAMETER,
		NONE
	};

	bool parseResponse(const char* buffer);
	bool parseRequestLine(const char* msg);
	bool parseSetParameter(std::string& message);
	bool parseSessionId(std::string& message);
	bool parseCSeq(std::string& message);

	Method getMethod() const
	{
		return _method;
	}

	uint32_t getCSeq() const
	{
		return _cseq_res;
	}

	int getSession() const
	{
		return _sessionId;
	}

	void setUserAgent(const char* userAgent)
	{
		_userAgent = std::string(userAgent);
	}

	void setRtspUrl(const char* url)
	{
		_rtspUrl = std::string(url);
	}

	int GetUIBCPort()
	{
		return m_uibcPort;
	}

	int buildOptionReq(const char* buf, int bufSize);
	int buildOptionRes(const char* buf, int bufSize);
	int buildGetParamterRes(const char* buf, int bufSize, uint32_t port);
	int buildGetParamterResEx(const char* buf, int bufSize);
	int buildSetParamterRes(const char* buf, int bufSize, int seq);
	int buildSetupUdpReq(const char* buf, int bufSize, uint16_t port);
	int buildPlayReq(const char* buf, int bufSize);
	int buildTeardown(const char* buf, int bufSize);
	int buildIdrReq(const char* buf, int bufSize);

	int extractPort(const std::string& input);

private:
	Method _method;
	uint32_t _cseq = 1;
	uint32_t _cseq_res = 1;
	std::string _userAgent;
	std::string _rtspUrl;
	uint32_t _sessionId = 0;
	int m_uibcPort;
};

#endif