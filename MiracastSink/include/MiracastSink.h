#pragma once

#include "WXBase.h"

#include <evpp/event_loop.h>
#include <evpp/buffer.h>
#include <evpp/tcp_client.h>
#include <evpp/tcp_conn.h>
#include <evpp/udp/sync_udp_client.h>
#include <evpp/udp/udp_server.h>

#include "RTSPResponse.h"
#include "WXSafeQueue.h"
#include "SDLPlayer.h"
#include "EventLoopManager.h"
#include "UIBCManager.h"

using namespace winrt::Windows::Media::Miracast;

class MiracastSink;

class TSParseWorker : public WXThread {
public:
	TSParseWorker(MiracastSink* owner);

	void ThreadPrepare() override;
	void ThreadProcess() override;
	void ThreadPost() override;

private:
	MiracastSink* m_owner;
	void* m_pTSDemux;
	FILE* fH264;
	FILE* fAAC;
};

class MiracastSink
{
public:
	MiracastSink();
	~MiracastSink();

	bool Start(std::string ip);
	void SetMiracastConn(MiracastReceiverConnection conn);
	void SetDeviceName(std::string name);

	WXFifo& TSDataBuffer();

	void OnData(int type, uint8_t* buffer, int bufSize);

	std::shared_ptr<UIBCManager>& GetUIBCManager();

private:
	std::string m_deviceName;
	MiracastReceiverConnection m_conn;
	shared_ptr<evpp::TCPClient> _tcpClient;
	std::shared_ptr<RtspResponse> _rtspResponsePtr;
	evpp::udp::Server _serverStream;
	evpp::udp::sync::Client _clientUdp;
	uint16_t _server_port = 0;
	uint16_t _reConnNums = 0;
	bool _initRtspUdp = false;
	bool _initMedia = false;
	bool _isConnected = false;

	void close();
	void onMessage(const evpp::TCPConnPtr& conn, evpp::Buffer* msg);
	void onConnection(const evpp::TCPConnPtr& conn);
	void sendMessage(std::shared_ptr<char> buf, uint32_t size);
	void onPayloadData(evpp::EventLoop* loop, evpp::udp::MessagePtr& msg);

	void handleCmdOption();
	void handleCmdGetParamter();
	void handleCmdSetParamter();
	void handleSetup();
	void handlePlay();
	void handleTearDown();
	bool setupRtpOverUdp();
	void requestIdr();

	bool generateRtpPort(uint16_t& port);

	std::string m_remoteIP;
	std::shared_ptr<UIBCManager> m_uibcManager;

	EventLoopManager m_eventLoopManager;
	WXFifo m_tsDataBuffer;
	void* m_pTSDemux;
	std::unique_ptr<TSParseWorker> m_tsParseWorker;
	std::shared_ptr<SDLPlayer> m_sdlPlayer;

	WXSafeQueue<std::vector<uint8_t>> m_videoDataQueue;
	WXSafeQueue<std::vector<uint8_t>> m_audioDataQueue;
};

