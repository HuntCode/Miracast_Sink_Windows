#include "pch.h"
#include "MiracastWiFiDirect.h"


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

MiracastWiFiDirect::MiracastWiFiDirect() : m_session(nullptr)
{

}

MiracastWiFiDirect::~MiracastWiFiDirect()
{
}

void MiracastWiFiDirect::Start()
{
    m_receiver.StatusChanged({ std::bind(&MiracastWiFiDirect::OnStatusChanged, this, std::placeholders::_1,std::placeholders::_2) });

    MiracastReceiverSettings settings = m_receiver.GetDefaultSettings();

    settings.FriendlyName(L"Miracast Windows Test");
    settings.AuthorizationMethod(MiracastReceiverAuthorizationMethod::None);
    settings.ModelName(m_receiver.GetDefaultSettings().ModelName());
    settings.ModelNumber(m_receiver.GetDefaultSettings().ModelNumber());
    settings.RequireAuthorizationFromKnownTransmitters(false);

    auto settings_sts = m_receiver.DisconnectAllAndApplySettings(settings);
    m_session = m_receiver.CreateSession(nullptr);
    m_session.AllowConnectionTakeover(true);
    //session_.MaxSimultaneousConnections(6);
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

void MiracastWiFiDirect::OnStatusChanged(MiracastReceiver sender, IInspectable args)
{
    if (m_receiver == nullptr)
        return;

    auto status = m_receiver.GetStatus().ListeningStatus();
    switch (status)
    {
    case MiracastReceiverListeningStatus::NotListening:
    {
        std::wcout << "MiracastReceiver NotListening" << std::endl;
        break;
    }
    case MiracastReceiverListeningStatus::Listening:
    {
        std::wcout << "MiracastReceiver Listening" << std::endl;
        break;
    }
    case MiracastReceiverListeningStatus::ConnectionPending:
    {
        std::wcout << "MiracastReceiver ConnectionPending" << std::endl;
        break;
    }
    case MiracastReceiverListeningStatus::Connected:
    {
        std::wcout << "MiracastReceiver Connected" << std::endl;
        break;
    }
    case MiracastReceiverListeningStatus::DisabledByPolicy:
    {
        std::wcout << "MiracastReceiver DisabledByPolicy" << std::endl;
        break;
    }
    case MiracastReceiverListeningStatus::TemporarilyDisabled:
    {
        std::wcout << "MiracastReceiver TemporarilyDisabled" << std::endl;
        break;
    }
    default:
        std::wcout << "Unknown MiracastReceiverListeningStatus" << std::endl;
        break;
    }

    std::wcout << "MaxSimultaneousConnections : " << m_receiver.GetStatus().MaxSimultaneousConnections() << std::endl;
}

void MiracastWiFiDirect::OnConnectionCreated(MiracastReceiverSession sender, MiracastReceiverConnectionCreatedEventArgs args)
{
    winrt::hstring name = args.Connection().Transmitter().Name();

    std::wcout << "MiracastWiFiDirect OnConnected" << std::endl;
    std::wcout << "Transmitter Name: " << name.c_str() << std::endl;
    std::wcout << "Transmitter Mac: " << args.Connection().Transmitter().MacAddress().c_str() << std::endl;

    MiracastReceiverConnection conn = args.Connection();

    std::thread th([conn, name, this](wchar_t* mac) {
        char* device_name = WcharToChar((wchar_t*)name.c_str());
        std::string remote_ip = GetIPByMac((char*)"arp -a", mac);
        if ("" != remote_ip)
        {
            m_miracastSinkMap[name].reset();
            std::shared_ptr<MiracastSink> miracastSink = std::make_shared<MiracastSink>();
            miracastSink->SetDeviceName(device_name);
            miracastSink->SetMiracastConn(conn);
            miracastSink->Start(remote_ip);
            m_miracastSinkMap[name] = miracastSink;
        }
        else
        {
            LOG(ERROR) << device_name << ": arp get ip error";
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

    auto iter = m_miracastSinkMap.find(name);
    if (iter != m_miracastSinkMap.end())
    {
        m_miracastSinkMap.erase(iter);
        std::wcout << "Delete Transmitter name: " << name.c_str() << std::endl;
    }
}

void MiracastWiFiDirect::OnMediaSourceCreated(MiracastReceiverSession sender, MiracastReceiverMediaSourceCreatedEventArgs args)
{
}
