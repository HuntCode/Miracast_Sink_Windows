#include <random>

#include "MiracastSink.h"

MiracastSink::MiracastSink()
{
	m_rtspManager = std::make_shared<RTSPManager>();
}

MiracastSink::~MiracastSink()
{
	Stop();
}

bool MiracastSink::Start(const std::string& ip)
{
	m_rtspManager->Start(ip);

	return true;
}

void MiracastSink::Stop()
{
	m_rtspManager->Stop();
}

void MiracastSink::SetDeviceName(std::string name)
{
	m_rtspManager->SetDeviceName(name);
}

void MiracastSink::SetStreamID(uint32_t streamId)
{
	m_rtspManager->SetStreamID(streamId);
}

void MiracastSink::SetMiracastCallback(std::shared_ptr<IMiracastCallback> callback)
{
	m_rtspManager->SetMiracastCallback(callback);
}

int MiracastSink::GetUIBCCategory()
{
	return m_rtspManager->GetUIBCCategory();
}

void MiracastSink::SendHIDMouse(unsigned char type, char xdiff, char ydiff, char wdiff)
{
	m_rtspManager->SendHIDMouse(type, xdiff, ydiff, wdiff);
}

void MiracastSink::SendHIDKeyboard(unsigned char type, unsigned char modType, unsigned short keyboardValue)
{
	m_rtspManager->SendHIDKeyboard(type, modType, keyboardValue);
}

void MiracastSink::SendHIDMultiTouch(const char* multiTouchMessage)
{
	m_rtspManager->SendHIDMultiTouch(multiTouchMessage);
}

bool MiracastSink::SupportMultiTouch()
{
	return m_rtspManager->SupportMultiTouch();
}

void MiracastSink::SendGenericTouch(const char* inEventDesc, double widthRatio, double heightRatio)
{
	return m_rtspManager->SendGenericTouch(inEventDesc, widthRatio, heightRatio);
}
