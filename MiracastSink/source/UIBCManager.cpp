#include "pch.h"
#include "UIBCManager.h"
#include <iostream>

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

void UIBCManager::SendHIDKeyboard(unsigned char type, unsigned char modType, unsigned short ascii)
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
                pending_keys[i] = ascii;
                break;
            }
        }
    }
    else if (type == KEYBOARD_UP) {
        for (int i = 0; i < MAX_KEYBOARD_REPORT; i++) {
            if (pending_keys[i] == ascii)
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
            char hidch = AsciiToHID(pending_keys[i]);
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

char UIBCManager::AsciiToHID(unsigned short ascii)
{
    /*   if (DFB_KEY_TYPE(ascii) == DIKT_FUNCTION) {
           return (0x3a + (ascii - DIKS_F1));
       }

       if (DFB_KEY_TYPE(ascii) == DIKT_SPECIAL) {
           if (ascii == DIKS_CURSOR_LEFT)
               return 0x50;
           if (ascii == DIKS_CURSOR_RIGHT)
               return 0x4f;
           if (ascii == DIKS_CURSOR_UP)
               return 0x52;
           if (ascii == DIKS_CURSOR_DOWN)
               return 0x51;
           if (ascii == DIKS_INSERT)
               return 0x49;
           if (ascii == DIKS_HOME)
               return 0x4a;
           if (ascii == DIKS_END)
               return 0x4d;
           if (ascii == DIKS_PAGE_UP)
               return 0x4b;
           if (ascii == DIKS_PAGE_DOWN)
               return 0x4e;
           return 0x00;
       }*/

       //if (DFB_KEY_TYPE(ascii) == DIKT_UNICODE) {
    if (ascii >= 'a' && ascii <= 'z')
        return (ascii - 'a' + 4);

    if (ascii >= 'A' && ascii <= 'Z')
        return (ascii - 'A' + 4);

    if (ascii >= '1' && ascii <= '9')
        return (ascii - '1' + 0x1e);

    if (ascii == '0' || ascii == ')')
        return 0x27;
    if (ascii == '(')
        return 0x26;
    if (ascii == '*')
        return 0x25;
    if (ascii == '&')
        return 0x24;
    if (ascii == '^')
        return 0x23;
    if (ascii == '%')
        return 0x22;
    if (ascii == '$')
        return 0x21;
    if (ascii == '#')
        return 0x20;
    if (ascii == '@')
        return 0x1f;
    if (ascii == '!')
        return 0x1e;
    if (ascii == 0x09) // tab
        return 0x2b;
    if (ascii == 0x08) // backspace
        return 0x2a;
    if (ascii == 0x0d) // newline
        return 0x28;
    if (ascii == 0x1b) // escape
        return 0x29;
    if (ascii == 127) // DEL
        return 0x4c;
    if (ascii == ' ') // space
        return 0x2c;

    if (ascii == '-' || ascii == '_')
        return 0x2d;
    if (ascii == '=' || ascii == '+')
        return 0x2e;
    if (ascii == '[' || ascii == '{')
        return 0x2f;
    if (ascii == ']' || ascii == '}')
        return 0x30;
    if (ascii == '\\' || ascii == '|')
        return 0x31;
    if (ascii == ';' || ascii == ':')
        return 0x33;
    if (ascii == '\'' || ascii == '"')
        return 0x34;
    if (ascii == '`' || ascii == '~')
        return 0x35;
    if (ascii == ',' || ascii == '<')
        return 0x36;
    if (ascii == '.' || ascii == '>')
        return 0x37;
    if (ascii == '/' || ascii == '?')
        return 0x38;
    // }
    return 0;
}