#include "pch.h"
#include "MiracastApp.h"

MiracastApp::MiracastApp() :m_start(false), m_miracastWifiDirect(nullptr)
{
	winrt::init_apartment();

	ImmDisableIME(-1);
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
	if (m_start)
		return;

	if (m_miracastWifiDirect == nullptr)
	{
		m_miracastWifiDirect = new MiracastWiFiDirect();
		m_miracastWifiDirect->Start();
	}

	m_start = true;
}

void MiracastApp::StopMiracast()
{
	{
		std::lock_guard<std::mutex> lg(m_mtxStop);

		if (!m_start)
			return;

		m_start = false;
	}

	if (m_miracastWifiDirect)
	{
		delete m_miracastWifiDirect;
		m_miracastWifiDirect = nullptr;
	}

}