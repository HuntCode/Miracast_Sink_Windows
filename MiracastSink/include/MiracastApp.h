#include "MiracastWiFiDirect.h"

#include <mutex>
#include "IMiracastCallback.h"
#ifdef ANDROID
#include "MiracastWifiDirect_Android.h"

#endif
class MiracastApp
{
public:
	static MiracastApp* instance();
	void StartMiracast();
	void StopMiracast();

	void SetMiracastCallback(std::shared_ptr<IMiracastCallback> callback);

	void Disconnect(uint32_t streamId);

	int GetUIBCCategory(uint32_t streamId);

	void SendHIDMouse(uint32_t streamId, unsigned char type, char xdiff, char ydiff, char wdiff);
	void SendHIDKeyboard(uint32_t streamId, unsigned char type, unsigned char modType, unsigned short keyboardValue);
	void SendHIDMultiTouch(uint32_t streamId, const char* multiTouchMessage);

	bool SupportMultiTouch(uint32_t streamId);

	// Generic
	void SendGenericTouch(uint32_t streamId, const char* inEventDesc, double widthRatio, double heightRatio);

	void AddAndroidDevice(std::string ip, std::string name);
private:
	MiracastApp();
	~MiracastApp();

	// 禁止拷贝构造和赋值
	MiracastApp(const MiracastApp&) = delete;
	MiracastApp& operator=(const MiracastApp&) = delete;

	std::mutex m_mtxStop;
	bool m_start;

#ifdef WIN32
	std::shared_ptr<MiracastWiFiDirect> m_miracastWifiDirect;
#endif

#ifdef ANDROID
	std::shared_ptr<MiracastWifiDirectAndroid> m_miracastAndroidWifiDirect;
#endif
};