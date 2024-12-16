#include "MiracastWiFiDirect.h"

#ifdef WIN32

char* WcharToChar(wchar_t* wc)
{
    int len = WideCharToMultiByte(CP_ACP, 0, wc, wcslen(wc), NULL, 0, NULL, NULL);
    char* m_char = new char[len + 1];
    WideCharToMultiByte(CP_ACP, 0, wc, wcslen(wc), m_char, len, NULL, NULL);
    m_char[len] = '\0';

    return m_char;
}

void Replace(char* str, char findChar, char replaceChar)
{
    for (int i = 0; str[i] != '\0'; i++)
    {
        if (str[i] == findChar)
            str[i] = replaceChar;
    }
}

std::string GetIPByMac(char* cmd, wchar_t* in_mac)
{
    std::string result = "";
    int num = 0;

    while (num < 15)
    {
        FILE* file;
        char ptr[1024] = { 0 };
        char tmp[1024] = { 0 };
        char ip[32] = { 0 };
        char mac[32] = { 0 };
        strcat_s(ptr, cmd);

        // 转换wchar_t*到char*
        char* temp_mac = WcharToChar(in_mac);

        // 截取前4段（前11个字符）和第2到第5段（从第3个字符开始的11个字符）
        char mac_prefix[12] = { 0 };
        strncpy(mac_prefix, temp_mac, 11);
        mac_prefix[11] = '\0';

        char mac_middle[12] = { 0 };
        strncpy(mac_middle, temp_mac + 3, 11);
        mac_middle[11] = '\0';

        delete[] temp_mac;  // 释放内存

        if ((file = _popen(ptr, "r")) != NULL)
        {
            while (fgets(tmp, 1024, file) != NULL)
            {
                sscanf(tmp, "%s   %s  ", ip, mac);
                Replace(mac, '-', ':');

                // 截取同样的两部分进行比较
                char mac_compare_prefix[12] = { 0 };
                strncpy(mac_compare_prefix, mac, 11);
                mac_compare_prefix[11] = '\0';

                char mac_compare_middle[12] = { 0 };
                strncpy(mac_compare_middle, mac + 3, 11);
                mac_compare_middle[11] = '\0';

                // 如果匹配前4段或第2到第5段，返回IP
                if (strcmp(mac_prefix, mac_compare_prefix) == 0 || strcmp(mac_middle, mac_compare_middle) == 0)
                {
                    result = ip;
                    _pclose(file);
                    return result;
                }
            }
            _pclose(file);
        }

        num++;
        Sleep(200);
    }

    return result;
}


static unsigned int IPToStreamID(const std::string& ipAddress) {
    struct in_addr addr;

    // 将 IP 地址字符串转换为网络字节序的二进制格式
    if (inet_pton(AF_INET, ipAddress.c_str(), &addr) == 1) {
        // 将网络字节序的地址转换为主机字节序的无符号整数
        return ntohl(addr.s_addr);
    }
    else {
        std::cerr << "Invalid IP address: " << ipAddress << std::endl;
        return 0;
    }
}

MiracastWiFiDirect::MiracastWiFiDirect() : m_session(nullptr)
{
}

MiracastWiFiDirect::~MiracastWiFiDirect()
{
}

void MiracastWiFiDirect::Start()
{
    m_statusChangedToken = m_receiver.StatusChanged({ std::bind(&MiracastWiFiDirect::OnStatusChanged, this, std::placeholders::_1,std::placeholders::_2) });
    
    std::cout << "MiracastReceiver support MaxSimultaneousConnections:" << m_receiver.GetStatus().MaxSimultaneousConnections() << std::endl;
    
    ProcessSetting();
    ProcessSession();
}

void MiracastWiFiDirect::Stop()
{
    m_session.Close();
    m_receiver.StatusChanged(m_statusChangedToken);
}

void MiracastWiFiDirect::Disconnect(uint32_t streamId)
{
    auto sink = GetSink(streamId);
    if (sink) {
        sink->Stop();
    }
}

void MiracastWiFiDirect::SetMiracastCallback(std::shared_ptr<IMiracastCallback> callback)
{
    m_miracastCallback = callback;
}

void MiracastWiFiDirect::OnConnect(uint32_t streamId)
{
    if (m_miracastCallback) {
        m_miracastCallback->OnConnect(streamId);
    }
}

void MiracastWiFiDirect::OnDisconnect(uint32_t streamId)
{
    if (m_miracastCallback) {
        m_miracastCallback->OnDisconnect(streamId);
    }

    // Note: 
    // 1)要在RTSP断开后通过回调关闭conn，否则某些手机，如vivo可能会进行WiFi Direct重连
    //   此时逻辑错误，conn被占用，后续投屏无法进行
    // 2)貌似依然低概率出现WiFi Direct重连，添加一个延迟，再关闭conn？
    Sleep(100);

    auto conn = GetConnection(streamId);
    if (conn){
        std::cout << "WiFiDirect Close Connection! streamId=" << streamId << std::endl;
        conn->Close();
        RemoveConnection(streamId);
    }
    else {
        std::cout << "WiFiDirect Connection not found! streamId=" << streamId << std::endl;
    }
        
}

void MiracastWiFiDirect::OnPlay(uint32_t streamId)
{
    if (m_miracastCallback) {
        m_miracastCallback->OnPlay(streamId);
    }
}

void MiracastWiFiDirect::OnData(uint32_t streamId, int type, uint8_t* buffer, int bufSize)
{
    if (m_miracastCallback) {
        m_miracastCallback->OnData(streamId, type, buffer, bufSize);
    }
}

int MiracastWiFiDirect::GetUIBCCategory(uint32_t streamId)
{
    auto sink = GetSink(streamId);
    if (sink) {
        return sink->GetUIBCCategory();
    }
    return -1; // not support
}

void MiracastWiFiDirect::SendHIDMouse(uint32_t streamId, unsigned char type, char xdiff, char ydiff, char wdiff)
{
    auto sink = GetSink(streamId);
    if (sink) {
        sink->SendHIDMouse(type, xdiff, ydiff, wdiff);
    } 
}

void MiracastWiFiDirect::SendHIDKeyboard(uint32_t streamId, unsigned char type, unsigned char modType, unsigned short keyboardValue)
{
    auto sink = GetSink(streamId);
    if (sink) {
        sink->SendHIDKeyboard(type, modType, keyboardValue);
    }
}

void MiracastWiFiDirect::SendHIDMultiTouch(uint32_t streamId, const char* multiTouchMessage)
{
    auto sink = GetSink(streamId);
    if (sink) {
        sink->SendHIDMultiTouch(multiTouchMessage);
    }
}

bool MiracastWiFiDirect::SupportMultiTouch(uint32_t streamId)
{
    auto sink = GetSink(streamId);
    if (sink) {
        return sink->SupportMultiTouch();
    }
    return false;
}

void MiracastWiFiDirect::SendGenericTouch(uint32_t streamId, const char* inEventDesc, double widthRatio, double heightRatio)
{
    auto sink = GetSink(streamId);
    if (sink) {
        sink->SendGenericTouch(inEventDesc, widthRatio, heightRatio);
    }
}

void MiracastWiFiDirect::OnStatusChanged(MiracastReceiver sender, IInspectable args)
{
    if (m_receiver == nullptr)
        return;

    auto status = m_receiver.GetStatus().ListeningStatus();
    switch (status)
    {
    case MiracastReceiverListeningStatus::NotListening:
    {
        std::wcout << "MiracastReceiverListeningStatus NotListening" << std::endl;
        break;
    }
    case MiracastReceiverListeningStatus::Listening:
    {
        // Note: 
        // 1）初始化时不会进入该状态
        // 2）已有设备连接时，后续设备只会触发connected状态
        // 3）只有最后一台设备断开，才会进入该状态
        // 最后清理conn的保底方法，避免低概率出现的conn占用情况，影响后续投屏
        for (auto it = m_connMap.begin(); it != m_connMap.end(); ++it) {
            std::shared_ptr<MiracastReceiverConnection> value = it->second;
            value->Close();
        }
        std::wcout << "m_connMap.clear()" << std::endl;
        m_connMap.clear();

        std::wcout << "MiracastReceiverListeningStatus Listening" << std::endl;
        break;
    }
    case MiracastReceiverListeningStatus::ConnectionPending:
    {
        std::wcout << "MiracastReceiverListeningStatus ConnectionPending" << std::endl;
        break;
    }
    case MiracastReceiverListeningStatus::Connected:
    {
        std::wcout << "MiracastReceiverListeningStatus Connected" << std::endl;
        break;
    }
    case MiracastReceiverListeningStatus::DisabledByPolicy:
    {
        std::wcout << "MiracastReceiverListeningStatus DisabledByPolicy" << std::endl;
        break;
    }
    case MiracastReceiverListeningStatus::TemporarilyDisabled:
    {
        std::wcout << "MiracastReceiverListeningStatus TemporarilyDisabled" << std::endl;
        break;
    }
    default:
        std::wcout << "Unknown MiracastReceiverListeningStatus" << std::endl;
        break;
    }
}

void MiracastWiFiDirect::OnConnectionCreated(MiracastReceiverSession sender, MiracastReceiverConnectionCreatedEventArgs args)
{
    winrt::hstring name = args.Connection().Transmitter().Name();

    std::wcout << "MiracastWiFiDirect OnConnected" << std::endl;
    std::wcout << "Transmitter Name: " << name.c_str() << std::endl;
    std::wcout << "Transmitter Mac: " << args.Connection().Transmitter().MacAddress().c_str() << std::endl;

    std::thread th([name, args, this](wchar_t* mac) {
        char* device_name = WcharToChar((wchar_t*)name.c_str());
        std::string remote_ip = GetIPByMac((char*)"arp -a", mac);
        if ("" != remote_ip)
        {
            uint32_t streamId = IPToStreamID(remote_ip);

            auto mirascast_sink = std::make_shared<MiracastSink>();
            mirascast_sink->SetDeviceName(device_name);
            mirascast_sink->SetStreamID(streamId);
            mirascast_sink->SetMiracastCallback(shared_from_this());
            mirascast_sink->Start(remote_ip);
    
            {
                std::lock_guard<std::mutex> lock(m_mapMutex);
                AddConnection(streamId, std::make_shared<MiracastReceiverConnection>(args.Connection()));
                AddSink(streamId, mirascast_sink);
            }
        }
        else
        {
            std::cout << device_name << ": ARP Get IP error" << std::endl;
            args.Connection().Close();
        }
        delete device_name;
        device_name = nullptr;
        }, (wchar_t*)args.Connection().Transmitter().MacAddress().c_str());
    th.detach();
}

void MiracastWiFiDirect::OnDisconnected(MiracastReceiverSession sender, MiracastReceiverDisconnectedEventArgs args)
{
    winrt::hstring name = args.Connection().Transmitter().Name();

    std::wcout << "MiracastWiFiDirect OnDisconnected" << std::endl;
    std::wcout << "Transmitter Name: " << name.c_str() << std::endl;
    std::wcout << "Transmitter Mac: " << args.Connection().Transmitter().MacAddress().c_str() << std::endl;

    std::thread th([name, this](wchar_t* mac) {
        char* device_name = WcharToChar((wchar_t*)name.c_str());
        std::string remote_ip = GetIPByMac((char*)"arp -a", mac);
        if ("" != remote_ip)
        {
            uint32_t streamId = IPToStreamID(remote_ip);
            {
                std::lock_guard<std::mutex> lock(m_mapMutex);
                RemoveSink(streamId);
            }
        }
        else
        {
            std::cout << device_name << ": ARP Get IP error" << std::endl;
        }
        delete device_name;
        device_name = nullptr;
        }, (wchar_t*)args.Connection().Transmitter().MacAddress().c_str());
    th.detach();
}

void MiracastWiFiDirect::OnMediaSourceCreated(MiracastReceiverSession sender, MiracastReceiverMediaSourceCreatedEventArgs args)
{
}

void MiracastWiFiDirect::ProcessSetting()
{
    MiracastReceiverSettings settings = m_receiver.GetDefaultSettings();
    settings.FriendlyName(L"Miracast Windows Test");
    settings.AuthorizationMethod(MiracastReceiverAuthorizationMethod::None);
    settings.ModelName(m_receiver.GetDefaultSettings().ModelName());
    settings.ModelNumber(m_receiver.GetDefaultSettings().ModelNumber());
    settings.RequireAuthorizationFromKnownTransmitters(false);
    auto settingsResult = m_receiver.DisconnectAllAndApplySettings(settings);
    auto settingsResultStatus = settingsResult.Status();
    switch (settingsResultStatus)
    {
    case MiracastReceiverApplySettingsStatus::Success:
    {
        std::wcout << "MiracastReceiverApplySettingsStatus Success" << std::endl;
        break;
    }
    case MiracastReceiverApplySettingsStatus::UnknownFailure:
    {
        std::wcout << "MiracastReceiverApplySettingsStatus UnknownFailure" << std::endl;
        break;
    }
    case MiracastReceiverApplySettingsStatus::MiracastNotSupported:
    {
        std::wcout << "MiracastReceiverApplySettingsStatus MiracastNotSupported" << std::endl;
        break;
    }
    case MiracastReceiverApplySettingsStatus::AccessDenied:
    {
        std::wcout << "MiracastReceiverApplySettingsStatus AccessDenied" << std::endl;
        break;
    }
    case MiracastReceiverApplySettingsStatus::FriendlyNameTooLong:
    {
        std::wcout << "MiracastReceiverApplySettingsStatus AccessDenied" << std::endl;
        break;
    }
    case MiracastReceiverApplySettingsStatus::ModelNameTooLong:
    {
        std::wcout << "MiracastReceiverApplySettingsStatus AccessDenied" << std::endl;
        break;
    }
    case MiracastReceiverApplySettingsStatus::ModelNumberTooLong:
    {
        std::wcout << "MiracastReceiverApplySettingsStatus AccessDenied" << std::endl;
        break;
    }
    case MiracastReceiverApplySettingsStatus::InvalidSettings:
    {
        std::wcout << "MiracastReceiverApplySettingsStatus AccessDenied" << std::endl;
        break;
    }
    default:
        std::wcout << "Unknown MiracastReceiverApplySettingsStatus" << std::endl;
        break;
    }
}

void MiracastWiFiDirect::ProcessSession()
{
    m_session = m_receiver.CreateSession(nullptr);
    m_session.AllowConnectionTakeover(false);
    m_session.MaxSimultaneousConnections(m_receiver.GetStatus().MaxSimultaneousConnections());
    m_session.ConnectionCreated({ std::bind(&MiracastWiFiDirect::OnConnectionCreated, this, std::placeholders::_1,std::placeholders::_2) });
    m_session.Disconnected({ std::bind(&MiracastWiFiDirect::OnDisconnected, this, std::placeholders::_1,std::placeholders::_2) });
    m_session.MediaSourceCreated({ std::bind(&MiracastWiFiDirect::OnMediaSourceCreated, this, std::placeholders::_1,std::placeholders::_2) });

    // m_session.Start()状态会阻塞，阻塞时长不确定
    std::thread([this]() {
        MiracastReceiverSessionStartResult result = m_session.Start();
        MiracastReceiverSessionStartStatus status = result.Status();

        switch (status)
        {
        case MiracastReceiverSessionStartStatus::Success:
        {
            std::wcout << "MiracastReceiverSessionStartStatus Success" << std::endl;
            break;
        }
        case MiracastReceiverSessionStartStatus::UnknownFailure:
        {
            std::wcout << "MiracastReceiverSessionStartStatus UnknownFailure" << std::endl;
            break;
        }
        case MiracastReceiverSessionStartStatus::MiracastNotSupported:
        {
            std::wcout << "MiracastReceiverSessionStartStatus MiracastNotSupported" << std::endl;
            break;
        }
        case MiracastReceiverSessionStartStatus::AccessDenied:
        {
            std::wcout << "MiracastReceiverSessionStartStatus AccessDenied" << std::endl;
            break;
        }
        default:
            std::wcout << "Unknown MiracastReceiverSessionStartStatus" << std::endl;
            break;
        }
        }).detach();
}

void MiracastWiFiDirect::AddConnection(uint32_t streamId, std::shared_ptr<MiracastReceiverConnection> conn)
{
    m_connMap[streamId] = conn;
}

void MiracastWiFiDirect::RemoveConnection(uint32_t streamId)
{
    m_connMap.erase(streamId);
}

std::shared_ptr<MiracastReceiverConnection> MiracastWiFiDirect::GetConnection(uint32_t streamId)
{
    auto it = m_connMap.find(streamId);
    if (it != m_connMap.end()) {
        return it->second;
    }
    return nullptr; // 如果没有找到，返回 nullptr
}

void MiracastWiFiDirect::AddSink(uint32_t streamId, std::shared_ptr<MiracastSink> sink)
{
    m_sinkMap[streamId] = std::move(sink);
}

void MiracastWiFiDirect::RemoveSink(uint32_t streamId)
{
    m_sinkMap.erase(streamId);
}

std::shared_ptr<MiracastSink> MiracastWiFiDirect::GetSink(uint32_t streamId)
{
    auto it = m_sinkMap.find(streamId);
    if (it != m_sinkMap.end()) {
        return it->second;
    }
    return nullptr; // 如果没有找到，返回 nullptr
}

#endif