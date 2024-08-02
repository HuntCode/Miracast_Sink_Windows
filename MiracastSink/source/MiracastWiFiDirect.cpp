#include "pch.h"
#include "MiracastWiFiDirect.h"

#define TS_TYPE_VIDEO 1
#define TS_TYPE_AUDIO 2

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

        char* temp_mac = WcharToChar(in_mac);
        temp_mac[11] = '\0';
        if ((file = _popen(ptr, "r")) != NULL)
        {
            while (fgets(tmp, 1024, file) != NULL)
            {
                sscanf(tmp, "%s   %s  ", ip, mac);
                Replace(mac, '-', ':');
                mac[11] = '\0';
                if (strcmp(temp_mac, mac) == 0)
                {
                    result = ip;
                    delete temp_mac;
                    _pclose(file);
                    return result;
                }
            }
            _pclose(file);
        }
        delete temp_mac;
        num++;

        Sleep(1000);
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

void MiracastWiFiDirect::OnConnect(uint32_t streamId)
{

}

void MiracastWiFiDirect::OnDisconnect(uint32_t streamId)
{
    std::lock_guard<std::mutex> lock(mapMutex);
    auto sdlPlayer = GetSDLPlayer(streamId);
    if (sdlPlayer) {
        sdlPlayer->Stop();
        RemoveSDLPlayer(streamId);
    }

    // Note: 
    // 1)要在RTSP断开后通过回调关闭conn，否则某些手机，如vivo可能会进行WiFi Direct重连
    //   此时逻辑错误，conn被占用，后续投屏无法进行
    // 2)貌似依然低概率出现WiFi Direct重连，添加一个延迟，再关闭conn？
    Sleep(100);

    auto conn = GetConnection(streamId);
    if (conn){
        conn->Close();
        RemoveConnection(streamId);
    }
        
}

void MiracastWiFiDirect::OnPlay(uint32_t streamId)
{
    std::thread([this, streamId]() {
        
        auto sink = GetSink(streamId);
        auto sdlPlayer = GetSDLPlayer(streamId);
        if (sdlPlayer == nullptr) {
            sdlPlayer = std::make_shared<SDLPlayer>(1920, 1080, sink);
            sdlPlayer->Init("SDLPlayer");

            std::lock_guard<std::mutex> lock(mapMutex);
            AddSDLPlayer(streamId, sdlPlayer);
        }


        sdlPlayer->Play();

        if (sink)
            sink->Stop();

        }).detach();
}

void MiracastWiFiDirect::OnData(uint32_t streamId, int type, uint8_t* buffer, int bufSize)
{
    auto sdlPlayer = GetSDLPlayer(streamId);
    if (sdlPlayer) {
        if (type == TS_TYPE_VIDEO) {
            sdlPlayer->ProcessVideo(buffer, bufSize);
        }
        else if (type == TS_TYPE_AUDIO) {
            sdlPlayer->ProcessAudio(buffer, bufSize);
        }
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
                std::lock_guard<std::mutex> lock(mapMutex);
                AddConnection(streamId, std::make_shared<MiracastReceiverConnection>(args.Connection()));
                AddSink(streamId, mirascast_sink);
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
                std::lock_guard<std::mutex> lock(mapMutex);
                RemoveSink(streamId);

                auto sdlPlayer = GetSDLPlayer(streamId);
                if (sdlPlayer) {
                    sdlPlayer->Stop();
                    RemoveSDLPlayer(streamId);
                }
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

void MiracastWiFiDirect::AddSDLPlayer(uint32_t streamId, std::shared_ptr<SDLPlayer> sdlPlayer)
{
    m_playerMap[streamId] = std::move(sdlPlayer);
}

void MiracastWiFiDirect::RemoveSDLPlayer(uint32_t streamId)
{
    m_playerMap.erase(streamId);
}

std::shared_ptr<SDLPlayer> MiracastWiFiDirect::GetSDLPlayer(uint32_t streamId)
{
    auto it = m_playerMap.find(streamId);
    if (it != m_playerMap.end()) {
        return it->second;
    }
    return nullptr; // 如果没有找到，返回 nullptr
}