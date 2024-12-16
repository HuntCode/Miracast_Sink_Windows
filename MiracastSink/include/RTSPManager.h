#ifndef RTSP_MANAGER_H
#define RTSP_MANAGER_H

#include <iostream>

#define HV_STATICLIB
#include <hv/TcpClient.h>

#include "RTPManager.h"
#include "UIBCManager.h"
#include "IMiracastCallback.h"

class RTSPManager
{
public:
	RTSPManager();
	~RTSPManager();

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

	void Start(const std::string& ip);
	void Stop();

	void SetDeviceName(std::string name);
	void SetStreamID(uint32_t streamId);
	void SetMiracastCallback(std::shared_ptr<IMiracastCallback> callback);

	int GetUIBCCategory();

	void SendHIDMouse(unsigned char type, char xdiff, char ydiff, char wdiff);
	void SendHIDKeyboard(unsigned char type, unsigned char modType, unsigned short keyboardValue);
	void SendHIDMultiTouch(const char* multiTouchMessage);

	bool SupportMultiTouch();

	// Generic
	void SendGenericTouch(const char* inEventDesc, double widthRatio, double heightRatio);

	void RequestIdr();
private:
	std::string m_sourceIP;
	std::string m_deviceName;
	uint32_t m_streamId;
	Method m_method;
	int m_cseq;
	std::string m_userAgent;
	std::string m_rtspUrl;
	uint32_t m_sessionID = 0;
	uint16_t m_rtpPort = 0;
	bool m_initRtpServer = false;
	int m_uibcPort;

	std::mutex m_mtxStop;
	bool m_start;

	std::shared_ptr<IMiracastCallback> m_miracastCallback;

	hv::TcpClient m_rtspClient;
	std::shared_ptr<RTPManager> m_rtpManager;
	std::shared_ptr<UIBCManager> m_uibcManager;

	void OnConnection(const hv::SocketChannelPtr& conn);
	void OnMessage(const hv::SocketChannelPtr& channel, hv::Buffer* msg);

	void ParseResponse(const char* buffer);
	bool ParseRequestLine(const char* msg);
	bool ParseSetParameter(std::string& msg);
	bool ParseSessionID(std::string& message);
	bool ParseCSeq(std::string& message);
	void ParseRTCPServerPort(std::string& message);
	
	void HandleOptions();
	void HandleGetParamter();
	void HandleSetParamter();
	void HandleSetup();
	void HandlePlay();
	void HandleTearDown();


	int BuildOptionResponse(const char* buf, int bufSize);
	int BuildOptionRequest(const char* buf, int bufSize);
	int BuildGetParamterResponseM3(const char* buf, int bufSize, uint32_t port);
	int BuildGetParamterResponseM16(const char* buf, int bufSize);
	int BuildSetParamterResponse(const char* buf, int bufSize, int seq);
	int BuildSetupRequest(const char* buf, int bufSize, uint16_t port);
	int BuildPlayRequest(const char* buf, int bufSize);
	int BuildTearDown(const char* buf, int bufSize);
	int BuildIdrRequest(const char* buf, int bufSize);

	void SendMsg(std::shared_ptr<char> buf, uint32_t size);

	void SetupRtpServer();

	int GetUIBCPort(const std::string& input);
	UIBCCategory CheckUIBCCategory(const std::string& input);

	Method GetMethod();
	uint32_t GetCSeq();
};

#endif // RTSP_MANAGER_H