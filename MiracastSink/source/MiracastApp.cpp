#include "MiracastApp.h"

MiracastApp::MiracastApp() :m_start(false)
{
#ifdef WIN32
	winrt::init_apartment();

	ImmDisableIME(-1);

	m_miracastWifiDirect = std::make_shared<MiracastWiFiDirect>();
#endif

#ifdef ANDROID
	m_miracastAndroidWifiDirect = std::make_shared<MiracastWifiDirectAndroid>();
#endif
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

#ifdef WIN32
	m_miracastWifiDirect->Start();
#endif

#ifdef ANDROID
	m_miracastAndroidWifiDirect->Start();
#endif
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
#ifdef WIN32

	m_miracastWifiDirect->Stop();
#endif


#ifdef ANDROID
	m_miracastAndroidWifiDirect->Stop();
#endif
}

void MiracastApp::SetMiracastCallback(std::shared_ptr<IMiracastCallback> callback)
{
#ifdef WIN32
	m_miracastWifiDirect->SetMiracastCallback(callback);
#endif

#ifdef ANDROID
	m_miracastAndroidWifiDirect->SetMiracastCallback(callback);
#endif
}

void MiracastApp::Disconnect(uint32_t streamId)
{
#ifdef WIN32
	m_miracastWifiDirect->Disconnect(streamId);
#endif

#ifdef ANDROID
	m_miracastAndroidWifiDirect->Disconnect(streamId);
#endif
}

int MiracastApp::GetUIBCCategory(uint32_t streamId)
{
#ifdef WIN32
	return m_miracastWifiDirect->GetUIBCCategory(streamId);
#endif
}

void MiracastApp::SendHIDMouse(uint32_t streamId, unsigned char type, char xdiff, char ydiff, char wdiff)
{
#ifdef WIN32
	m_miracastWifiDirect->SendHIDMouse(streamId, type, xdiff, ydiff, wdiff);
#endif

#ifdef ANDROID
	m_miracastAndroidWifiDirect->SendHIDMouse(streamId, type, xdiff, ydiff);
#endif
}

void MiracastApp::SendHIDKeyboard(uint32_t streamId, unsigned char type, unsigned char modType, unsigned short keyboardValue)
{
#ifdef WIN32
	m_miracastWifiDirect->SendHIDKeyboard(streamId, type, modType, keyboardValue);
#endif

#ifdef ANDROID
	m_miracastAndroidWifiDirect->SendHIDKeyboard(streamId, type, modType, keyboardValue);
#endif
}

void MiracastApp::SendHIDMultiTouch(uint32_t streamId, const char* multiTouchMessage)
{
#ifdef WIN32
	m_miracastWifiDirect->SendHIDMultiTouch(streamId, multiTouchMessage);
#endif

#ifdef ANDROID
	m_miracastAndroidWifiDirect->SendHIDMultiTouch(streamId, multiTouchMessage);
#endif
}

bool MiracastApp::SupportMultiTouch(uint32_t streamId)
{
#ifdef WIN32
	return m_miracastWifiDirect->SupportMultiTouch(streamId);
#endif

#ifdef ANDROID
	return m_miracastAndroidWifiDirect->SupportMultiTouch(streamId);
#endif
}


void MiracastApp::SendGenericTouch(uint32_t streamId, const char* inEventDesc, double widthRatio, double heightRatio)
{
#ifdef WIN32
	m_miracastWifiDirect->SendGenericTouch(streamId, inEventDesc, widthRatio, heightRatio);
#endif
}

void MiracastApp::AddAndroidDevice(std::string ip, std::string name) {
#ifdef ANDROID
	m_miracastAndroidWifiDirect->AttachSinkInfo(ip, name);
#endif
}

