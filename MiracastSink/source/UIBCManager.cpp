#include "UIBCManager.h"
#include <iostream>
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

#include "Util/logger.h"

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
	m_uibcFd(-1), 
    m_uibcEnable(true),
    m_uibcCategory(UIBC_NotSupport),
    m_HIDType(0)
{
#ifdef _WIN32
    InitWindowsSockets();
#endif

    for (int i = 0; i < MAX_KEYBOARD_REPORT; i++)
        m_pendingKeys[i] = 0;
    m_shiftDown = false;
    m_altDown = false;
    m_ctrlDown = false;
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

    if (GetUIBCCategory() == UIBC_HIDC) {
        if (m_HIDType & HIDTypeFlags_Keyboard ) {
            SendHIDDescriptor(HID_TYPE_KEYBOARD, s_keyboardDescriptor, sizeof(s_keyboardDescriptor));
        }
        if (m_HIDType & HIDTypeFlags_Mouse) {
            SendHIDDescriptor(HID_TYPE_MOUSE, s_mouseDescriptor, sizeof(s_mouseDescriptor));
        }
        if (m_HIDType & HIDTypeFlags_MultiTouch) {
            SendHIDDescriptor(HID_TYPE_MULTITOUCH, s_multiTouchDescriptor, sizeof(s_multiTouchDescriptor));

            // send multiTouch feature
            SendMultiTouchFeature();
        }
    }

    return 0;
}

bool UIBCManager::Connected()
{
	if (m_uibcFd > 0)
		return true;
	else
		return false;
}

void UIBCManager::SetEnable(bool enable)
{
    m_uibcEnable = enable;
}

bool UIBCManager::GetEnable()
{
    return m_uibcEnable;
}

void UIBCManager::SetUIBCCategory(UIBCCategory category)
{
    m_uibcCategory = category;
}

UIBCCategory UIBCManager::GetUIBCCategory()
{
    return m_uibcCategory;
}

void UIBCManager::SetHIDType(uint8_t typeFlags)
{
    m_HIDType = typeFlags;
}

uint8_t UIBCManager::GetHIDType()
{
    return m_HIDType;
}

bool UIBCManager::SupportMultiTouch()
{
    if (m_HIDType & HIDTypeFlags_MultiTouch) {
        return true;
    }
    return false;
}

void UIBCManager::SendHIDMouse(unsigned char type, char xdiff, char ydiff, char wdiff)
{
    if (!m_uibcEnable) {
        return;
    }

    if (type == kMouseLeftButtonDown) {
        m_lastButtons |= 0x01;
    }
    else if (type == kMouseMiddleButtonDown) {
        m_lastButtons |= 0x04;
    }
    else if (type == kMouseRightButtonDown) {
        m_lastButtons |= 0x02;
    }
    else if (type == kMouseLeftButtonUp ||
        type == kMouseMiddleButtonUp ||
        type == kMouseRightButtonUp) {
        m_lastButtons = 0;
    }
        

    unsigned char* inputMouse = (unsigned char*)malloc(HIDC_INPUT_MOUSE_TOTAL);
    memset(inputMouse, 0, HIDC_INPUT_MOUSE_TOTAL);
    // hidc mouse report
    inputMouse[0] = 1;   // usb
    inputMouse[1] = 1;   // mouse
    inputMouse[2] = 0;   // contains report
    unsigned short len = htons(HIDC_INPUT_MOUSE_BODY_LENGTH);
    memcpy(inputMouse + 3, &len, sizeof(len));
    inputMouse[5] = 0x28;
    inputMouse[6] = m_lastButtons;
    inputMouse[7] = xdiff;
    inputMouse[8] = ydiff;
    inputMouse[9] = wdiff;
    SendUIBCEvent(INPUT_HIDC, inputMouse, HIDC_INPUT_MOUSE_TOTAL);
    free(inputMouse);
}

void UIBCManager::SendHIDKeyboard(unsigned char type, unsigned char modType, unsigned short keyboardValue)
{
    if (!m_uibcEnable) {
        return;
    }

    if (modType == MODIFY_SHIFT) {
        if (type == KEYBOARD_DOWN)
            m_shiftDown = true;
        else
            m_shiftDown = false;
    }
    else if (modType == MODIFY_CTRL) {
        if (type == KEYBOARD_DOWN)
            m_ctrlDown = true;
        else
            m_ctrlDown = false;
    }
    else if (modType == MODIFY_ALT) {
        if (type == KEYBOARD_DOWN)
            m_altDown = true;
        else
            m_altDown = false;
    }

    if (type == KEYBOARD_DOWN) {
        for (int i = 0; i < MAX_KEYBOARD_REPORT; i++) {
            if (m_pendingKeys[i] == 0) {
                m_pendingKeys[i] = keyboardValue;
                break;
            }
        }
    }
    else if (type == KEYBOARD_UP) {
        for (int i = 0; i < MAX_KEYBOARD_REPORT; i++) {
            if (m_pendingKeys[i] == keyboardValue)
                m_pendingKeys[i] = 0;
        }
    }
    else
        return;

    unsigned char* inputKeyboard = (unsigned char*)malloc(HIDC_INPUT_KEYBOARD_TOTAL);
    memset(inputKeyboard, 0, HIDC_INPUT_KEYBOARD_TOTAL);

    inputKeyboard[0] = 1;   // usb
    inputKeyboard[1] = 0;   // keyboard
    inputKeyboard[2] = 0;   // contains report
    unsigned short len = htons(HIDC_INPUT_KEYBOARD_BODY_LENGTH);
    memcpy(inputKeyboard + 3, &len, sizeof(len));
    // [Report ID][Modifier Byte][Reserved Byte][Key 1][Key 2][Key 3][Key 4][Key 5][Key 6]
    // next, keyboard modifier
    inputKeyboard[5] = 0x29;
    inputKeyboard[6] = 0;
    if (m_shiftDown)
        inputKeyboard[6] |= 0x02;
    if (m_altDown)
        inputKeyboard[6] |= 0x04;
    if (m_ctrlDown)
        inputKeyboard[6] |= 0x01;
    // reserved byte
    inputKeyboard[7] = 0;

    int stindex = 8;
    for (int i = 0; i < MAX_KEYBOARD_REPORT; i++) {
        inputKeyboard[i + 8] = 0x00;
        if (m_pendingKeys[i] != 0) {
            char hidch = KeyboardValueToHID(m_pendingKeys[i]);
            if (hidch != 0) {
                inputKeyboard[stindex] = hidch;
                stindex++;
            }
        }
    }
    SendUIBCEvent(INPUT_HIDC, (unsigned char*)inputKeyboard, HIDC_INPUT_KEYBOARD_TOTAL);
    free(inputKeyboard);
}

/*
*  multiTouchMessage json格式
*  {
*       "fingers": 2, //最多支持10指触控
*       "timestamp": 6000,//相对值
*       "width": 1920,
*       "height": 1080,
*       "touch_data":[
*                {
*                   "touch_id": 0,
*                   "touch_state": 1, //1表示touch down，0表示touch up
*                   "x": 100,
*                   "y": 200
*                },
*                {
*                   "touch_id": 1,
*                   "touch_state": 1,
*                   "x": 150,
*                   "y": 250
*                }
*         ]
*  }
*/
void UIBCManager::SendHIDMultiTouch(const char* multiTouchMessage)
{
    if (!m_uibcEnable) {
        return;
    }

    unsigned char* inputMultiTouch = (unsigned char*)malloc(HIDC_INPUT_MULTITOUCH_TOTAL);
    memset(inputMultiTouch, 0, HIDC_INPUT_MULTITOUCH_TOTAL);

    inputMultiTouch[0] = 1;   // usb
    inputMultiTouch[1] = 3;   // multi touch
    inputMultiTouch[2] = 0;   // contains report
    unsigned short len = htons(HIDC_INPUT_MULTITOUCH_BODY_LENGTH);
    memcpy(inputMultiTouch + 3, &len, sizeof(len));

    inputMultiTouch[5] = 0x13;

    rapidjson::Document rapidDocument;
    rapidDocument.Parse(multiTouchMessage);
    if (rapidDocument.HasParseError()) {
        return;
    }

    unsigned short width;
    unsigned short height;
    if (rapidDocument.HasMember("width") && rapidDocument["width"].IsInt() &&
        rapidDocument.HasMember("height") && rapidDocument["height"].IsInt()) {
        width = rapidDocument["width"].GetInt();
        height = rapidDocument["height"].GetInt();
    }

    // touch_data
    if (rapidDocument.HasMember("touch_data") && rapidDocument["touch_data"].IsArray()) {
        const rapidjson::Value& touchData = rapidDocument["touch_data"];
        for (int i = 0; i < touchData.Size(); ++i) {
            const rapidjson::Value& touch = touchData[i];
            if (touch.HasMember("touch_id") && touch["touch_id"].IsInt() &&
                touch.HasMember("touch_state") && touch["touch_state"].IsInt() &&
                touch.HasMember("x") && touch["x"].IsInt() &&
                touch.HasMember("y") && touch["y"].IsInt()) {
                
                inputMultiTouch[6 + 6 * i] = touch["touch_state"].GetInt();
                inputMultiTouch[6 + 1 + 6 * i] = touch["touch_id"].GetInt();

                unsigned short x = touch["x"].GetInt();        
                unsigned short logicX = LOGICAL_MAXIMUM * x / width;
                //DebugL << "x: " << x << " logicX: " << logicX;

                inputMultiTouch[6 + 2 + 6 * i] = logicX & 0xFF;
                inputMultiTouch[6 + 3 + 6 * i] = (logicX >> 8) & 0xFF;


                unsigned short y = touch["y"].GetInt();
                unsigned short logicY = LOGICAL_MAXIMUM * y / height;
                //DebugL << "y: " << x << " logicY: " << logicX;

                inputMultiTouch[6 + 4 + 6 * i] = logicY & 0xFF;
                inputMultiTouch[6 + 5 + 6 * i] = (logicY >> 8) & 0xFF;

            }
        }
    }

    // finger count
    if (rapidDocument.HasMember("fingers") && rapidDocument["fingers"].IsInt()) {
        inputMultiTouch[HIDC_INPUT_MULTITOUCH_TOTAL - 3] = rapidDocument["fingers"].GetInt();
    }

    // timestamp
    if (rapidDocument.HasMember("timestamp") && rapidDocument["timestamp"].IsInt()) {
        int timestamp = rapidDocument["timestamp"].GetInt();
        
        unsigned short feature = timestamp;
        inputMultiTouch[HIDC_INPUT_MULTITOUCH_TOTAL - 2] = feature & 0xFF;
        inputMultiTouch[HIDC_INPUT_MULTITOUCH_TOTAL - 1] = (feature >> 8) & 0xFF;
    }

    SendUIBCEvent(INPUT_HIDC, inputMultiTouch, HIDC_INPUT_MULTITOUCH_TOTAL);
    free(inputMultiTouch);
}

void UIBCManager::SendMultiTouchFeature()
{
    unsigned char* inputMouse = (unsigned char*)malloc(HIDC_INPUT_MULTITOUCH_FEATURE_TOTAL);
    memset(inputMouse, 0, HIDC_INPUT_MULTITOUCH_FEATURE_TOTAL);
    // hidc multi touch report
    inputMouse[0] = 1;   // usb
    inputMouse[1] = 3;   // multi touch
    inputMouse[2] = 0;   // contains report
    unsigned short len = htons(3);
    memcpy(inputMouse + 3, &len, sizeof(len));
    inputMouse[5] = 0x12;

    unsigned short feature = HIDC_INPUT_MULTITOUCH_FINGERS;
    inputMouse[6] = feature & 0xFF;
    inputMouse[7] = (feature >> 8) & 0xFF;

    SendUIBCEvent(INPUT_HIDC, inputMouse, HIDC_INPUT_MULTITOUCH_FEATURE_TOTAL);
    free(inputMouse);
}

/* Generic Category
*/
/*
*  {
*      "input_type": 0, //GENERIC_TOUCH_DOWN/GENERIC_TOUCH_UP/GENERIC_TOUCH_MOVE
*      "fingers": 1, //最多支持10指触控
*      "touch_data" : [
*          {
*             "touch_id": 0,
*             "x" : 100,
*             "y" : 200
*          }
*       ]
*   }
*/
void UIBCManager::SendGenericTouch(const char* inEventDesc, double widthRatio, double heightRatio)
{
    if (!m_uibcEnable) {
        return;
    }

    rapidjson::Document rapidDocument;
    rapidDocument.Parse(inEventDesc);
    if (rapidDocument.HasParseError()) {
        return;
    }
    int32_t numberOfPointers = 0;
    // finger count
    if (rapidDocument.HasMember("fingers") && rapidDocument["fingers"].IsInt()) {
        numberOfPointers = rapidDocument["fingers"].GetInt();
    }

    int32_t genericPacketLen = 0;
    genericPacketLen = 3 + 1 + numberOfPointers * 5;

    unsigned char* inputGenericTouch = (unsigned char*)malloc(genericPacketLen);
    memset(inputGenericTouch, 0, genericPacketLen);

    int32_t inputType = 0;
    if (rapidDocument.HasMember("input_type") && rapidDocument["input_type"].IsInt()) {
        inputType = rapidDocument["input_type"].GetInt();
    }
    //Generic Input Body Format
    inputGenericTouch[0] = inputType & 0xFF; // Type ID, 1 octet

    // Length, 2 bytes
    inputGenericTouch[1] = (genericPacketLen >> 8) & 0xFF; // first byte
    inputGenericTouch[2] = genericPacketLen & 0xFF; //last byte

    // Number of pointers, 1 octet
    inputGenericTouch[3] = numberOfPointers & 0xFF;

    // touch_data
    if (rapidDocument.HasMember("touch_data") && rapidDocument["touch_data"].IsArray()) {
        const rapidjson::Value& touchData = rapidDocument["touch_data"];
        for (int i = 0; i < touchData.Size(); ++i) {
            const rapidjson::Value& touch = touchData[i];
            if (touch.HasMember("touch_id") && touch["touch_id"].IsInt() &&
                touch.HasMember("x") && touch["x"].IsInt() &&
                touch.HasMember("y") && touch["y"].IsInt()) {

                inputGenericTouch[4 + 5 * i] = touch["touch_id"].GetInt();

                unsigned short x = touch["x"].GetInt();
                inputGenericTouch[4 + 1 + 5 * i] = (x >> 8) & 0xFF;
                inputGenericTouch[4 + 2 + 5 * i] = x & 0xFF;

                unsigned short y = touch["y"].GetInt();
                inputGenericTouch[4 + 3 + 5 * i] = (y >> 8) & 0xFF;
                inputGenericTouch[4 + 4 + 5 * i] = y & 0xFF;
            }
        }
    }

    SendUIBCEvent(INPUT_GENERIC, inputGenericTouch, genericPacketLen);
    free(inputGenericTouch);
}


/*
*  {
*      "input_type": 3, //GENERIC_KEY_DOWN/GENERIC_KEY_UP
*      "key_one" : 111,
*      "key_two" : 222,
*   }
*/
void UIBCManager::SendGenericKey(const char* inEventDesc)
{
    if (!m_uibcEnable) {
        return;
    }

    rapidjson::Document rapidDocument;
    rapidDocument.Parse(inEventDesc);
    if (rapidDocument.HasParseError()) {
        return;
    }

    int32_t genericPacketLen = 0;
    genericPacketLen = 3 + 5;

    unsigned char* inputGenericKey = (unsigned char*)malloc(genericPacketLen);
    memset(inputGenericKey, 0, genericPacketLen);

    int32_t inputType = 0;
    if (rapidDocument.HasMember("input_type") && rapidDocument["input_type"].IsInt()) {
        inputType = rapidDocument["input_type"].GetInt();
    }

    //Generic Input Body Format
    inputGenericKey[0] = inputType & 0xFF; // Type ID, 1 octet
    
    //Length, 2 bytes
    inputGenericKey[1] = (genericPacketLen >> 8) & 0xFF; // first byte
    inputGenericKey[2] = genericPacketLen & 0xFF; // last byte
    inputGenericKey[3] = 0x00; // reserved


    int32_t key1 = 0;
    if (rapidDocument.HasMember("key_one") && rapidDocument["key_one"].IsInt()) {
        key1 = rapidDocument["key_one"].GetInt();
    }
    inputGenericKey[4] = (key1 >> 8) & 0xFF;
    inputGenericKey[5] = key1 & 0xFF;

    int32_t key2 = 0;
    if (rapidDocument.HasMember("key_two") && rapidDocument["key_two"].IsInt()) {
        key2 = rapidDocument["key_two"].GetInt();
    }
    inputGenericKey[6] = (key2 >> 8) & 0xFF;
    inputGenericKey[7] = key2 & 0xFF;

    SendUIBCEvent(INPUT_GENERIC, inputGenericKey, genericPacketLen);
    free(inputGenericKey);
}

/*
*  {
*      "input_type": 5, //GENERIC_ZOOM
*      "x" : 111,
*      "y" : 222,
*      "integer_part": 1,
*      "fraction_part": 256, //The unit of the fractional part shall be 1/256, 
*                            //and the sign of the fractional part is always positive.            
*   }
*/
void UIBCManager::SendGenericZoom(const char* inEventDesc)
{
    if (!m_uibcEnable) {
        return;
    }

    rapidjson::Document rapidDocument;
    rapidDocument.Parse(inEventDesc);
    if (rapidDocument.HasParseError()) {
        return;
    }

    int32_t genericPacketLen = 0;
    genericPacketLen = 3 + 6;

    unsigned char* inputGenericZoom = (unsigned char*)malloc(genericPacketLen);
    memset(inputGenericZoom, 0, genericPacketLen);

    int32_t inputType = 0;
    if (rapidDocument.HasMember("input_type") && rapidDocument["input_type"].IsInt()) {
        inputType = rapidDocument["input_type"].GetInt();
    }

    //Generic Input Body Format
    inputGenericZoom[0] = inputType & 0xFF; // Type ID, 1 octet
    inputGenericZoom[1] = (genericPacketLen >> 8) & 0xFF; // Length, 2 octets
    inputGenericZoom[2] = genericPacketLen & 0xFF; // Length, 2 octets

    int32_t xCoord = 0;
    if (rapidDocument.HasMember("x") && rapidDocument["x"].IsInt()) {
        xCoord = rapidDocument["x"].GetInt();
    }
    inputGenericZoom[3] = (xCoord >> 8) & 0xFF;
    inputGenericZoom[4] = xCoord & 0xFF;

    int32_t yCoord = 0;
    if (rapidDocument.HasMember("y") && rapidDocument["y"].IsInt()) {
        yCoord = rapidDocument["y"].GetInt();
    }
    inputGenericZoom[5] = (yCoord >> 8) & 0xFF;
    inputGenericZoom[6] = yCoord & 0xFF;

    int32_t integerPart = 0;
    if (rapidDocument.HasMember("integer_part") && rapidDocument["integer_part"].IsInt()) {
        integerPart = rapidDocument["integer_part"].GetInt();
    }
    inputGenericZoom[7] = integerPart & 0xFF;

    int32_t fractionPart = 0;
    if (rapidDocument.HasMember("fraction_part") && rapidDocument["fraction_part"].IsInt()) {
        fractionPart = rapidDocument["fraction_part"].GetInt();
    }
    inputGenericZoom[8] = fractionPart & 0xFF;

    SendUIBCEvent(INPUT_GENERIC, inputGenericZoom, genericPacketLen);
    free(inputGenericZoom);
}

/*
*  {
*      "input_type": 6, //GENERIC_VERTICAL_SCROLL/GENERIC_HORIZONTAL_SCROLL
*      "unit" : 0, //0000 0000 01 00 0000(64)/0000 0000 0000 0000(0)
*      "direction" : 0,//0000 0000 00 1 0 0000(32)/0000 0000 0000 0000(0)
*      "amount_to_scroll": 1, //000 1 1111 1111 1111
*   }
*/
void UIBCManager::SendGenericScroll(const char* inEventDesc)
{
    if (!m_uibcEnable) {
        return;
    }

    rapidjson::Document rapidDocument;
    rapidDocument.Parse(inEventDesc);
    if (rapidDocument.HasParseError()) {
        return;
    }

    int32_t genericPacketLen = 0;
    genericPacketLen = 3 + 2;

    unsigned char* inputGenericScroll = (unsigned char*)malloc(genericPacketLen);
    memset(inputGenericScroll, 0, genericPacketLen);

    int32_t inputType = 0;
    if (rapidDocument.HasMember("input_type") && rapidDocument["input_type"].IsInt()) {
        inputType = rapidDocument["input_type"].GetInt();
    }

    //Generic Input Body Format
    inputGenericScroll[0] = inputType & 0xFF; // Type ID, 1 octet
    inputGenericScroll[1] = (genericPacketLen >> 8) & 0xFF; // Length, 2 octets
    inputGenericScroll[2] = genericPacketLen & 0xFF; // Length, 2 octets
    inputGenericScroll[3] = 0x00; // Clear the byte
    inputGenericScroll[4] = 0x00; // Clear the byte
    /*
        B15B14; Scroll Unit Indication bits.
        0b00; the unit is a pixel (normalized with respect to the WFD Source display resolution that is conveyed in an RTSP M4 request message).
        0b01; the unit is a mouse notch (where the application is responsible for representing the number of pixels per notch).
        0b10-0b11; Reserved.

        B13; Scroll Direction Indication bit.
        0b0; Scrolling to the right. Scrolling to the right means the displayed content being shifted to the left from a user perspective.
        0b1; Scrolling to the left. Scrolling to the left means the displayed content being shifted to the right from a user perspective.

        B12:B0; Number of Scroll bits.
        Number of units for a Horizontal scroll.
    */

    int32_t unit = 0;
    if (rapidDocument.HasMember("unit") && rapidDocument["unit"].IsInt()) {
        unit = rapidDocument["unit"].GetInt();
    }
    inputGenericScroll[3] = unit & 0xFF;

    int32_t direction = 0;
    if (rapidDocument.HasMember("direction") && rapidDocument["direction"].IsInt()) {
        direction = rapidDocument["direction"].GetInt();
    }
    inputGenericScroll[3] |= (direction & 0xFF);

    int32_t amount = 0;
    if (rapidDocument.HasMember("amount_to_scroll") && rapidDocument["amount_to_scroll"].IsInt()) {
        amount = rapidDocument["amount_to_scroll"].GetInt();
    }
    inputGenericScroll[3] |= ((amount >> 8) & 0xFF); 
    inputGenericScroll[4] = (amount & 0xFF);

    SendUIBCEvent(INPUT_GENERIC, inputGenericScroll, genericPacketLen);
    free(inputGenericScroll);
}

/*
*  {
*      "input_type": 8, //GENERIC_ROTATE
*      "integer_part" : 1,  //The signed integer portion of the amount to rotate in units in radians. A negative 
*                           //number indicates to rotate clockwise; a positive number indicates to rotate 
*                           //counter-clockwise
*      "fraction_part" : 256, //The unit of the fractional part shall be 1/256, and the sign of the fractional part is 
*                             //always positive.
*   }
*/
void UIBCManager::SendGenericRotate(const char* inEventDesc)
{
    if (!m_uibcEnable) {
        return;
    }

    rapidjson::Document rapidDocument;
    rapidDocument.Parse(inEventDesc);
    if (rapidDocument.HasParseError()) {
        return;
    }

    int32_t genericPacketLen = 0;
    genericPacketLen = 3 + 2;

    unsigned char* inputGenericRotate = (unsigned char*)malloc(genericPacketLen);
    memset(inputGenericRotate, 0, genericPacketLen);

    int32_t inputType = 0;
    if (rapidDocument.HasMember("input_type") && rapidDocument["input_type"].IsInt()) {
        inputType = rapidDocument["input_type"].GetInt();
    }

    //Generic Input Body Format
    inputGenericRotate[0] = inputType & 0xFF; // Type ID, 1 octet
    inputGenericRotate[1] = (genericPacketLen >> 8) & 0xFF; // Length, 2 octets
    inputGenericRotate[2] = genericPacketLen & 0xFF; // Length, 2 octets
 
    int32_t integerPart = 0;
    if (rapidDocument.HasMember("integer_part") && rapidDocument["integer_part"].IsInt()) {
        integerPart = rapidDocument["integer_part"].GetInt();
    }
    inputGenericRotate[3] = integerPart & 0xFF;

    int32_t fractionPart = 0;
    if (rapidDocument.HasMember("fraction_part") && rapidDocument["fraction_part"].IsInt()) {
        fractionPart = rapidDocument["fraction_part"].GetInt();
    }
    inputGenericRotate[4] = fractionPart & 0xFF;

    SendUIBCEvent(INPUT_GENERIC, inputGenericRotate, genericPacketLen);
    free(inputGenericRotate);
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
    inputBodyHeader[1] = type;  // descriptor type
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