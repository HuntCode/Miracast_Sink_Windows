#ifndef __MIRACASTWIFIDIRECT_H_
#define __MIRACASTWIFIDIRECT_H_

#ifdef WIN32
#include <unordered_map>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Media.Miracast.h>

#include "MiracastSink.h"
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

	void Disconnect(uint32_t streamId);

	// 传递回调给上层
	void SetMiracastCallback(std::shared_ptr<IMiracastCallback> callback);

	/* IMiracastCallback */
	virtual void OnConnect(uint32_t streamId) override;
	virtual void OnDisconnect(uint32_t streamId) override;
	virtual void OnPlay(uint32_t streamId) override;
	virtual void OnData(uint32_t streamId, int type, uint8_t* buffer, int bufSize) override;


	int GetUIBCCategory(uint32_t streamId);

	void SendHIDMouse(uint32_t streamId, unsigned char type, char xdiff, char ydiff, char wdiff);
	void SendHIDKeyboard(uint32_t streamId, unsigned char type, unsigned char modType, unsigned short keyboardValue);
	void SendHIDMultiTouch(uint32_t streamId, const char* multiTouchMessage);

	bool SupportMultiTouch(uint32_t streamId);

	// Generic
	void SendGenericTouch(uint32_t streamId, const char* inEventDesc, double widthRatio, double heightRatio);

private:
	std::shared_ptr<IMiracastCallback> m_miracastCallback;

	// 定义两个 unordered_map 容器
	std::unordered_map<uint32_t, std::shared_ptr<MiracastReceiverConnection>> m_connMap;
	std::unordered_map<uint32_t, std::shared_ptr<MiracastSink>> m_sinkMap;
	std::mutex m_mapMutex;

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
};

#endif

#endif