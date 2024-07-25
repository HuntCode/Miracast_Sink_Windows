#pragma once
#include <unordered_map>
#include "MiracastSink.h"

using namespace winrt::Windows::Foundation;

class MiracastWiFiDirect
{
public:
	MiracastWiFiDirect();
	~MiracastWiFiDirect();
	void Start();

private:
	std::unordered_map<winrt::hstring, std::shared_ptr<MiracastSink>> m_miracastSinkMap;
	MiracastReceiver m_receiver;
	MiracastReceiverSession m_session;
	void OnStatusChanged(MiracastReceiver sender, IInspectable args);
	void OnConnectionCreated(MiracastReceiverSession sender, MiracastReceiverConnectionCreatedEventArgs args);
	void OnDisconnected(MiracastReceiverSession sender, MiracastReceiverDisconnectedEventArgs args);
	void OnMediaSourceCreated(MiracastReceiverSession sender, MiracastReceiverMediaSourceCreatedEventArgs args);
};