#include "pch.h"
#include "MiracastApp.h"
#include "SDLPlayer.h"

using namespace winrt;
using namespace Windows::Foundation;

int main()
{
#if defined(WIN32) || defined(_WIN32) 
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData))
	{
		WSACleanup();
	}
#endif

	google::InitGoogleLogging("");
	google::SetStderrLogging(google::GLOG_ERROR);

	MiracastApp::instance()->StartMiracast();

	// 阻塞主线程，直到输入 'q' 字符
	char input;
	std::cout << "Enter 'q' to quit the application." << std::endl;
	while (std::cin >> input) {
		if (input == 'q') {
			break;
		}
		std::cout << "Invalid input. Enter 'q' to quit the application." << std::endl;
	}

	MiracastApp::instance()->StopMiracast();

	return 0;
}
