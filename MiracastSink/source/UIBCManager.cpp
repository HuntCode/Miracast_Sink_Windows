#include "pch.h"
#include "UIBCManager.h"
#include <iostream>

#include <map>

// 定义一个键盘值到HID使用码的映射表
const std::map<unsigned short, char> keyboardToHIDMap = {
    // 字母
    {'a', 0x04}, {'b', 0x05}, {'c', 0x06}, {'d', 0x07}, {'e', 0x08},
    {'f', 0x09}, {'g', 0x0A}, {'h', 0x0B}, {'i', 0x0C}, {'j', 0x0D},
    {'k', 0x0E}, {'l', 0x0F}, {'m', 0x10}, {'n', 0x11}, {'o', 0x12},
    {'p', 0x13}, {'q', 0x14}, {'r', 0x15}, {'s', 0x16}, {'t', 0x17},
    {'u', 0x18}, {'v', 0x19}, {'w', 0x1A}, {'x', 0x1B}, {'y', 0x1C},
    {'z', 0x1D},

    // 大写字母（同小写字母）
    {'A', 0x04}, {'B', 0x05}, {'C', 0x06}, {'D', 0x07}, {'E', 0x08},
    {'F', 0x09}, {'G', 0x0A}, {'H', 0x0B}, {'I', 0x0C}, {'J', 0x0D},
    {'K', 0x0E}, {'L', 0x0F}, {'M', 0x10}, {'N', 0x11}, {'O', 0x12},
    {'P', 0x13}, {'Q', 0x14}, {'R', 0x15}, {'S', 0x16}, {'T', 0x17},
    {'U', 0x18}, {'V', 0x19}, {'W', 0x1A}, {'X', 0x1B}, {'Y', 0x1C},
    {'Z', 0x1D},

    // 数字
    {'1', 0x1E}, {'2', 0x1F}, {'3', 0x20}, {'4', 0x21}, {'5', 0x22},
    {'6', 0x23}, {'7', 0x24}, {'8', 0x25}, {'9', 0x26}, {'0', 0x27},

    // 符号
    {'!', 0x1E}, {'@', 0x1F}, {'#', 0x20}, {'$', 0x21}, {'%', 0x22},
    {'^', 0x23}, {'&', 0x24}, {'*', 0x25}, {'(', 0x26}, {')', 0x27},
    {'-', 0x2D}, {'_', 0x2D}, {'=', 0x2E}, {'+', 0x2E}, {'[', 0x2F},
    {'{', 0x2F}, {']', 0x30}, {'}', 0x30}, {'\\', 0x31}, {'|', 0x31},
    {';', 0x33}, {':', 0x33}, {'\'', 0x34}, {'"', 0x34}, {'`', 0x35},
    {'~', 0x35}, {',', 0x36}, {'<', 0x36}, {'.', 0x37}, {'>', 0x37},
    {'/', 0x38}, {'?', 0x38},

    // 控制键
    {0x08, 0x2A}, // Backspace
    {0x09, 0x2B}, // Tab
    {0x0D, 0x28}, // Enter
    {0x1B, 0x29}, // Escape
    {0x20, 0x2C}, // Space

    // 功能键，从0x0100开始
    {HH_FUNCTION_F1, 0x3A}, // F1
    {HH_FUNCTION_F2, 0x3B}, // F2
    {HH_FUNCTION_F3, 0x3C}, // F3
    {HH_FUNCTION_F4, 0x3D}, // F4
    {HH_FUNCTION_F5, 0x3E}, // F5
    {HH_FUNCTION_F6, 0x3F}, // F6
    {HH_FUNCTION_F7, 0x40}, // F7
    {HH_FUNCTION_F8, 0x41}, // F8
    {HH_FUNCTION_F9, 0x42}, // F9
    {HH_FUNCTION_F10, 0x43}, // F10
    {HH_FUNCTION_F11, 0x44}, // F11
    {HH_FUNCTION_F12, 0x45}, // F12

    // 方向键，从0x0110开始
    {HH_SPECIAL_LEFT_ARROW, 0x50},  // Left Arrow
    {HH_SPECIAL_RIGHT_ARROW, 0x4F}, // Right Arrow
    {HH_SPECIAL_UP_ARROW, 0x52},    // Up Arrow
    {HH_SPECIAL_DOWN_ARROW, 0x51},  // Down Arrow

    // 编辑键，从0x0120开始
    {HH_SPECIAL_INSERT, 0x49},      // Insert
    {HH_SPECIAL_HOME, 0x4A},        // Home
    {HH_SPECIAL_END, 0x4D},         // End
    {HH_SPECIAL_PAGEUP, 0x4B},      // Page Up
    {HH_SPECIAL_PAGEDOWN, 0x4E},    // Page Down

    // 特殊键，从0x0130开始
    {HH_SPECIAL_CAPS_LOCK, 0x39},   // Caps Lock
    {HH_SPECIAL_NUM_LOCK, 0x53},    // Num Lock
    {HH_SPECIAL_PRINT_SCREEN, 0x46}, // Print Screen
    {HH_SPECIAL_SCROLL_LOCK, 0x47}, // Scroll Lock
    {HH_SPECIAL_PAUSE, 0x48}, // Pause

    // 小键盘上的符号键，从0x0140开始
    {HH_SPECIAL_KP_DIVIDE, 0x54},   // 小键盘 /
    {HH_SPECIAL_KP_MULTIPLY, 0x55}, // 小键盘 *
    {HH_SPECIAL_KP_MINUS, 0x56},    // 小键盘 -
    {HH_SPECIAL_KP_PLUS, 0x57}, // 小键盘 +
    {HH_SPECIAL_KP_ENTER, 0x58}, // 小键盘 Enter

    // 小键盘上的数字键，从0x0150开始
    {HH_SPECIAL_KP_1, 0x59}, // 小键盘 1
    {HH_SPECIAL_KP_2, 0x5A}, // 小键盘 2
    {HH_SPECIAL_KP_3, 0x5B}, // 小键盘 3
    {HH_SPECIAL_KP_4, 0x5C}, // 小键盘 4
    {HH_SPECIAL_KP_5, 0x5D}, // 小键盘 5
    {HH_SPECIAL_KP_6, 0x5E}, // 小键盘 6
    {HH_SPECIAL_KP_7, 0x5F}, // 小键盘 7
    {HH_SPECIAL_KP_8, 0x60}, // 小键盘 8
    {HH_SPECIAL_KP_9, 0x61}, // 小键盘 9
    {HH_SPECIAL_KP_0, 0x62}, // 小键盘 0
    {HH_SPECIAL_KP_PERIOD, 0x63}, // 小键盘 .

    // 删除键
    {127, 0x4C} // Delete
};


UIBCManager::UIBCManager():
	m_uibcFd(-1)
{
#ifdef _WIN32
    InitWindowsSockets();
#endif

    for (int i = 0; i < MAX_KEYBOARD_REPORT; i++)
        pending_keys[i] = 0;
    shift_down = false;
    alt_down = false;
    ctrl_down = false;
}

UIBCManager::~UIBCManager()
{
#ifdef _WIN32
    CleanupWindowsSockets();
#endif
}

int UIBCManager::ConnectUIBC(const std::string& ip, int port)
{
    int sockfd;
    struct sockaddr_in serv_addr;
    struct in_addr server_addr;
    if (inet_pton(AF_INET, ip.c_str(), &server_addr) <= 0) {
        fprintf(stderr, "ERROR, invalid IP address\n");
        return -1;
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0) {
        perror("ERROR opening socket");
        return -1;
    }

#ifdef _WIN32
    ZeroMemory((char*)&serv_addr, sizeof(serv_addr));
#else
    bzero((char*)&serv_addr, sizeof(serv_addr));
#endif

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr = server_addr;
    //memcpy((char*)&serv_addr.sin_addr.s_addr, (char*)server->h_addr, server->h_length);
    serv_addr.sin_port = htons(port);

    if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("ERROR UIBC connecting");
        return -1;
    }

    std::cout << "UIBC connected..." << std::endl;

    m_uibcFd = sockfd;

    SendHIDDescriptor(HID_TYPE_MOUSE, mouseDescriptor, sizeof(mouseDescriptor));
    SendHIDDescriptor(HID_TYPE_KEYBOARD, keyboardDescriptor, sizeof(keyboardDescriptor));

    return 0;
}

bool UIBCManager::Connected()
{
	if (m_uibcFd > 0)
		return true;
	else
		return false;
}

void UIBCManager::SendHIDMouse(unsigned char type, char xdiff, char ydiff)
{
    if (type == MOUSE_BUTTON_DOWN) {
        last_buttons |= 0x01;
    }
    else if (type == MOUSE_BUTTON_UP)
        last_buttons = 0;

    unsigned char* inputMouse = (unsigned char*)malloc(HIDC_INPUT_MOUSE_TOTAL);
    memset(inputMouse, 0, HIDC_INPUT_MOUSE_TOTAL);
    // hidc mouse report
    inputMouse[0] = 1;   // usb
    inputMouse[1] = 1;   // mouse
    inputMouse[2] = 0;   // contains report
    unsigned short len = htons(HIDC_INPUT_MOUSE_BODY_LENGTH);
    memcpy(inputMouse + 3, &len, sizeof(len));
    inputMouse[5] = 0x28;
    inputMouse[6] = last_buttons;
    inputMouse[7] = xdiff;
    inputMouse[8] = ydiff;
    SendUIBCEvent(INPUT_HIDC, inputMouse, HIDC_INPUT_MOUSE_TOTAL);
    free(inputMouse);
}

void UIBCManager::SendHIDKeyboard(unsigned char type, unsigned char modType, unsigned short keyboardValue)
{
    if (modType == MODIFY_SHIFT) {
        if (type == KEYBOARD_DOWN)
            shift_down = true;
        else
            shift_down = false;
    }
    else if (modType == MODIFY_CTRL) {
        if (type == KEYBOARD_DOWN)
            ctrl_down = true;
        else
            ctrl_down = false;
    }
    else if (modType == MODIFY_ALT) {
        if (type == KEYBOARD_DOWN)
            alt_down = true;
        else
            alt_down = false;
    }

    if (type == KEYBOARD_DOWN) {
        for (int i = 0; i < MAX_KEYBOARD_REPORT; i++) {
            if (pending_keys[i] == 0) {
                pending_keys[i] = keyboardValue;
                break;
            }
        }
    }
    else if (type == KEYBOARD_UP) {
        for (int i = 0; i < MAX_KEYBOARD_REPORT; i++) {
            if (pending_keys[i] == keyboardValue)
                pending_keys[i] = 0;
        }
    }
    else
        return;

    unsigned char* inputKeyboard = (unsigned char*)malloc(HIDC_INPUT_KEYBOARD_TOTAL);
    memset(inputKeyboard, 0, HIDC_INPUT_KEYBOARD_TOTAL);

    inputKeyboard[0] = 1;   // usb
    inputKeyboard[1] = 0;   // mouse
    inputKeyboard[2] = 0;   // contains report
    unsigned short len = htons(HIDC_INPUT_KEYBOARD_BODY_LENGTH);
    memcpy(inputKeyboard + 3, &len, sizeof(len));
    // [Report ID][Modifier Byte][Reserved Byte][Key 1][Key 2][Key 3][Key 4][Key 5][Key 6]
    // next, keyboard modifier
    inputKeyboard[5] = 0x29;
    inputKeyboard[6] = 0;
    if (shift_down)
        inputKeyboard[6] |= 0x02;
    if (alt_down)
        inputKeyboard[6] |= 0x04;
    if (ctrl_down)
        inputKeyboard[6] |= 0x01;
    // reserved byte
    inputKeyboard[7] = 0;

    int stindex = 8;
    for (int i = 0; i < MAX_KEYBOARD_REPORT; i++) {
        inputKeyboard[i + 8] = 0x00;
        if (pending_keys[i] != 0) {
            char hidch = KeyboardValueToHID(pending_keys[i]);
            if (hidch != 0) {
                inputKeyboard[stindex] = hidch;
                stindex++;
            }
        }
    }
    SendUIBCEvent(INPUT_HIDC, (unsigned char*)inputKeyboard, HIDC_INPUT_KEYBOARD_TOTAL);
    free(inputKeyboard);
}

#ifdef _WIN32
#pragma comment(lib, "Ws2_32.lib")
int UIBCManager::InitWindowsSockets()
{
    WSADATA wsaData;
    return WSAStartup(MAKEWORD(2, 2), &wsaData);
}

void UIBCManager::CleanupWindowsSockets()
{
    WSACleanup();
}
#endif

void UIBCManager::SendHIDDescriptor(unsigned char type, unsigned char* body, unsigned short length)
{
    if (!Connected())
        return;

    // UIBC Header(4) + InputBody Header(5) + InputBody Length
    int entireLen = 4 + 5 + length;

    unsigned short header[2];
    char* bytes = (char*)&header[0];
    bytes[0] = 0; bytes[1] = 1;

    header[1] = htons(entireLen);

    unsigned char inputBodyHeader[5];
    inputBodyHeader[0] = 1;     // usb
    inputBodyHeader[1] = type;  // mouse
    inputBodyHeader[2] = 1;     // contains report   
    unsigned short len = htons(length);
    memcpy(inputBodyHeader + 3, &len, sizeof(len));

    char* payload = (char*)malloc(entireLen);
    memset(payload, 0, entireLen);

    memcpy(payload, &header, sizeof(header));
    memcpy(payload + sizeof(header), inputBodyHeader, sizeof(inputBodyHeader));
    memcpy(payload + sizeof(header) + sizeof(inputBodyHeader), body, length);
    
    int n = send(m_uibcFd, payload, entireLen, 0);
    free(payload);
}

void UIBCManager::SendUIBCEvent(unsigned char category, unsigned char* body, unsigned short length)
{
    if (!Connected())
        return;

    bool pad = false;
    int paddedlen = length + 4;
    if (paddedlen & 0x00000001)
        paddedlen += 1;

    char* padbody = (char*)malloc(paddedlen);
    memset(padbody, 0, paddedlen);

    unsigned short header[2];
    if (category == INPUT_HIDC) {
        // HIDC
        char* bytes = (char*)&header[0];
        bytes[0] = 0; bytes[1] = 1;
    }
    else
        header[0] = 0;

    header[1] = htons(paddedlen);
    memcpy(padbody, &header, sizeof(header));
    memcpy(padbody + sizeof(header), body, length);

    //write(sources.uibcfd, padbody, paddedlen);
    int n = send(m_uibcFd, padbody, paddedlen, 0);
    free(padbody);
}

char UIBCManager::KeyboardValueToHID(unsigned short keyboardValue)
{
    auto it = keyboardToHIDMap.find(keyboardValue);
    if (it != keyboardToHIDMap.end())
    {
        return it->second;
    }
    return 0;
}