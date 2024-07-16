#include "pch.h"
#include "MiracastApp.h"

MiracastApp::MiracastApp():m_miracastWifiDirect(nullptr)
{
	winrt::init_apartment();
}

MiracastApp::~MiracastApp()
{
}

MiracastApp* MiracastApp::instance()
{
	static MiracastApp* miracastApp = new MiracastApp();
	return miracastApp;
}

void MiracastApp::StartMiracast()
{
	m_eventLoopManager.Start();

	if (m_miracastWifiDirect == nullptr)
	{
		m_miracastWifiDirect = new MiracastWiFiDirect();
		m_miracastWifiDirect->Start(m_eventLoopManager.GetLoop());
	}
}

void MiracastApp::StopMiracast()
{
	m_eventLoopManager.Stop();

	if (m_miracastWifiDirect)
	{
		delete m_miracastWifiDirect;
		m_miracastWifiDirect = nullptr;
	}
}