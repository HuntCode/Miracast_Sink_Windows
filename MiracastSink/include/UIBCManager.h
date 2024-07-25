#ifndef HH_UIBCMANAGER_H
#define HH_UIBCMANAGER_H

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#else
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#endif

#define KEYBOARD_DOWN          2
#define KEYBOARD_UP            3

#define MODIFY_NONE            0
#define MODIFY_SHIFT           1
#define MODIFY_CTRL            2
#define MODIFY_ALT             3

#define INPUT_GENERIC          0
#define INPUT_HIDC             1

#define MOUSE_BUTTON_DOWN      0
#define MOUSE_BUTTON_UP        1
#define MOUSE_MOTION           2
#define MOUSE_WHEEL		       3

#define HID_TYPE_KEYBOARD   0
#define HID_TYPE_MOUSE      1

#define HIDC_INPUT_HEADER_LENGTH         5
#define HIDC_INPUT_MOUSE_BODY_LENGTH     6
#define MAX_KEYBOARD_REPORT              6
#define HIDC_INPUT_KEYBOARD_BODY_LENGTH  (1 + 1 + 1 + MAX_KEYBOARD_REPORT)
#define HIDC_INPUT_MOUSE_TOTAL (HIDC_INPUT_HEADER_LENGTH + HIDC_INPUT_MOUSE_BODY_LENGTH)
#define HIDC_INPUT_KEYBOARD_TOTAL (HIDC_INPUT_HEADER_LENGTH + HIDC_INPUT_KEYBOARD_BODY_LENGTH)


static unsigned char mouseDescriptor[] = {
    0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
    0x09, 0x02,        // Usage (Mouse)
    0xA1, 0x01,        // Collection (Application)
    0x85, 0x28,        //   Report ID (0x28)
    0x09, 0x01,        //   Usage (Pointer)
    0xA1, 0x00,        //   Collection (Physical)
    0x05, 0x09,        //     Usage Page (Button)
    0x19, 0x01,        //     Usage Minimum (Button 1)
    0x29, 0x08,        //     Usage Maximum (Button 8)
    0x15, 0x00,        //     Logical Minimum (0)
    0x25, 0x01,        //     Logical Maximum (1)
    0x95, 0x08,        //     Report Count (8)
    0x75, 0x01,        //     Report Size (1)
    0x81, 0x02,        //     Input (Data,Var,Abs)
    0x05, 0x01,        //     Usage Page (Generic Desktop Ctrls)
    0x09, 0x30,        //     Usage (X)
    0x09, 0x31,        //     Usage (Y)
    0x09, 0x38,        //     Usage (Wheel)
    0x0A, 0x38, 0x02,  //     Usage (AC Pan)
    0x15, 0x81,        //     Logical Minimum (-127)
    0x25, 0x7F,        //     Logical Maximum (127)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x04,        //     Report Count (4)
    0x81, 0x06,        //     Input (Data,Var,Rel)
    0xC0,              //   End Collection
    0xC0               // End Collection
};

static unsigned char keyboardDescriptor[] = {
    0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
    0x09, 0x06,        // Usage (Keyboard)
    0xA1, 0x01,        // Collection (Application)
    0x85, 0x29,        //   Report ID (41)
    0x05, 0x07,        //   Usage Page (Keyboard/Keypad)
    0x19, 0xE0,        //   Usage Minimum (0xE0)
    0x29, 0xE7,        //   Usage Maximum (0xE7)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x01,        //   Logical Maximum (1)
    0x75, 0x01,        //   Report Size (1)
    0x95, 0x08,        //   Report Count (8)
    0x81, 0x02,        //   Input (Data,Var,Abs)
    0x95, 0x01,        //   Report Count (1)
    0x75, 0x08,        //   Report Size (8)
    0x81, 0x03,        //   Input (Cnst,Var,Abs)
    0x95, 0x05,        //   Report Count (5)
    0x75, 0x01,        //   Report Size (1)
    0x05, 0x08,        //   Usage Page (LEDs)
    0x19, 0x01,        //   Usage Minimum (1)
    0x29, 0x05,        //   Usage Maximum (5)
    0x91, 0x02,        //   Output (Data,Var,Abs)
    0x95, 0x01,        //   Report Count (1)
    0x75, 0x03,        //   Report Size (3)
    0x91, 0x03,        //   Output (Cnst,Var,Abs)
    0x95, 0x06,        //   Report Count (6)
    0x75, 0x08,        //   Report Size (8)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x65,        //   Logical Maximum (101)
    0x05, 0x07,        //   Usage Page (Keyboard/Keypad)
    0x19, 0x00,        //   Usage Minimum (0)
    0x29, 0x65,        //   Usage Maximum (101)
    0x81, 0x00,        //   Input (Data,Ary,Abs)
    0xC0               // End Collection
};

class UIBCManager
{
public:
	UIBCManager();
	~UIBCManager();

    int ConnectUIBC(const std::string &ip, int port);
	bool Connected();

    void SendHIDMouse(unsigned char type, char xdiff, char ydiff);
    void SendHIDKeyboard(unsigned char type, unsigned char modType, unsigned short ascii);

private:
	int m_uibcFd;
	unsigned short pending_keys[MAX_KEYBOARD_REPORT];
	bool shift_down;
	bool alt_down;
	bool ctrl_down;
    unsigned char last_buttons = 0;

#ifdef _WIN32
    // Windows-specific socket initialization and cleanup functions
    int InitWindowsSockets();
    void CleanupWindowsSockets();
#endif

    void SendHIDDescriptor(unsigned char type, unsigned char* body, unsigned short length);
    void SendUIBCEvent(unsigned char category, unsigned char* body, unsigned short length);
    // return HID usage ID
    char AsciiToHID(unsigned short ascii);
};

#endif // HH_UIBCMANAGER_H