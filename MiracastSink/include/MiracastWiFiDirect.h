#pragma once
#include <unordered_map>

#include "MiracastSink.h"
#include "SDLPlayer.h"
#include "IMiracastCallback.h"

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Media::Miracast;

class MiracastWiFiDirect : public IMiracastCallback, public std::enable_shared_from_this<MiracastWiFiDirect>
{
public:
	MiracastWiFiDirect();
	~MiracastWiFiDirect();
	void Start();
	void Stop();

	/* IMiracastCallback */
	virtual void OnConnect(uint32_t streamId) override;
	virtual void OnDisconnect(uint32_t streamId) override;
	virtual void OnPlay(uint32_t streamId) override;
	virtual void OnData(uint32_t streamId, int type, uint8_t* buffer, int bufSize) override;

private:
	// 定义两个 unordered_map 容器
	std::unordered_map<uint32_t, std::shared_ptr<MiracastReceiverConnection>> m_connMap;
	std::unordered_map<uint32_t, std::shared_ptr<MiracastSink>> m_sinkMap;
	std::unordered_map<uint32_t, std::shared_ptr<SDLPlayer>> m_playerMap;
	std::mutex mapMutex;

	winrt::event_token m_statusChangedToken;
	MiracastReceiver m_receiver;
	MiracastReceiverSession m_session;

	void OnStatusChanged(MiracastReceiver sender, IInspectable args);
	void OnConnectionCreated(MiracastReceiverSession sender, MiracastReceiverConnectionCreatedEventArgs args);
	void OnDisconnected(MiracastReceiverSession sender, MiracastReceiverDisconnectedEventArgs args);
	void OnMediaSourceCreated(MiracastReceiverSession sender, MiracastReceiverMediaSourceCreatedEventArgs args);

	void ProcessSetting();
	void ProcessSession();

	void AddConnection(uint32_t streamId, std::shared_ptr<MiracastReceiverConnection> conn);
	void RemoveConnection(uint32_t streamId);
	std::shared_ptr<MiracastReceiverConnection> GetConnection(uint32_t streamId);

	void AddSink(uint32_t streamId, std::shared_ptr<MiracastSink> sink);
	void RemoveSink(uint32_t streamId);
	std::shared_ptr<MiracastSink> GetSink(uint32_t streamId);

	void AddSDLPlayer(uint32_t streamId, std::shared_ptr<SDLPlayer> sdlPlayer);
	void RemoveSDLPlayer(uint32_t streamId);
	std::shared_ptr<SDLPlayer> GetSDLPlayer(uint32_t streamId);
};