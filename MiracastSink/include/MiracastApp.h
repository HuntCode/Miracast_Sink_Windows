#pragma once
#include "EventLoopManager.h"
#include "MiracastWiFiDirect.h"

class MiracastApp
{
public:
	static MiracastApp* instance();
	void StartMiracast();
	void StopMiracast();

private:
	MiracastApp();
	~MiracastApp();

	// 禁止拷贝构造和赋值
	MiracastApp(const MiracastApp&) = delete;
	MiracastApp& operator=(const MiracastApp&) = delete;

	std::mutex m_mtxStop;
	bool m_start;
	EventLoopManager m_eventLoopManager;
	MiracastWiFiDirect* m_miracastWifiDirect;
};