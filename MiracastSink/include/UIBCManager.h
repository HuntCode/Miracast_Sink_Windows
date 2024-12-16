#ifndef HH_UIBCMANAGER_H
#define HH_UIBCMANAGER_H

#include <iostream>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#else
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#endif

// Mouse enum
enum {
    kMouseLeftButtonDown = 0,
    kMouseLeftButtonUp,
    kMouseMiddleButtonDown,
    kMouseMiddleButtonUp,
    kMouseRightButtonDown,
    kMouseRightButtonUp,
    kMouseMotion,
    kMouseWheel,
};

// Keyboard defines
#define KEYBOARD_DOWN          0
#define KEYBOARD_UP            1

#define MODIFY_NONE            0
#define MODIFY_SHIFT           1
#define MODIFY_CTRL            2
#define MODIFY_ALT             3

#define HH_FUNCTION_F1 0x0100     /* Function key (F1 - Fn) */
#define HH_FUNCTION_F2 (HH_FUNCTION_F1 + 1)
#define HH_FUNCTION_F3 (HH_FUNCTION_F1 + 2)
#define HH_FUNCTION_F4 (HH_FUNCTION_F1 + 3)
#define HH_FUNCTION_F5 (HH_FUNCTION_F1 + 4)
#define HH_FUNCTION_F6 (HH_FUNCTION_F1 + 5)
#define HH_FUNCTION_F7 (HH_FUNCTION_F1 + 6)
#define HH_FUNCTION_F8 (HH_FUNCTION_F1 + 7)
#define HH_FUNCTION_F9 (HH_FUNCTION_F1 + 8)
#define HH_FUNCTION_F10 (HH_FUNCTION_F1 + 9)
#define HH_FUNCTION_F11 (HH_FUNCTION_F1 + 10)
#define HH_FUNCTION_F12 (HH_FUNCTION_F1 + 11)

#define HH_SPECIAL_LEFT_ARROW   0x0110     /* Special key (e.g. Cursor Up or Menu) */
#define HH_SPECIAL_RIGHT_ARROW  0x0111
#define HH_SPECIAL_UP_ARROW     0x0112
#define HH_SPECIAL_DOWN_ARROW   0x0113
#define HH_SPECIAL_INSERT       0x0120
#define HH_SPECIAL_HOME         0x0121
#define HH_SPECIAL_END          0x0122
#define HH_SPECIAL_PAGEUP       0x0123
#define HH_SPECIAL_PAGEDOWN     0x0124
#define HH_SPECIAL_CAPS_LOCK    0x0130
#define HH_SPECIAL_NUM_LOCK     0x0131
#define HH_SPECIAL_PRINT_SCREEN 0x0132
#define HH_SPECIAL_SCROLL_LOCK  0x0133
#define HH_SPECIAL_PAUSE        0x0134

// KeyPad
#define HH_SPECIAL_KP_DIVIDE    0x0140
#define HH_SPECIAL_KP_MULTIPLY  0x0141
#define HH_SPECIAL_KP_MINUS     0x0142
#define HH_SPECIAL_KP_PLUS      0x0143
#define HH_SPECIAL_KP_ENTER     0x0144
#define HH_SPECIAL_KP_1         0x0150
#define HH_SPECIAL_KP_2         0x0151
#define HH_SPECIAL_KP_3         0x0152
#define HH_SPECIAL_KP_4         0x0153
#define HH_SPECIAL_KP_5         0x0154
#define HH_SPECIAL_KP_6         0x0155
#define HH_SPECIAL_KP_7         0x0156
#define HH_SPECIAL_KP_8         0x0157
#define HH_SPECIAL_KP_9         0x0158
#define HH_SPECIAL_KP_0         0x0159
#define HH_SPECIAL_KP_PERIOD    0x015A

// HIDC defines
#define INPUT_GENERIC          0
#define INPUT_HIDC             1

#define HID_TYPE_KEYBOARD       0
#define HID_TYPE_MOUSE          1
#define HID_TYPE_MULTITOUCH     3

#define HIDC_INPUT_HEADER_LENGTH         5
#define HIDC_INPUT_MOUSE_BODY_LENGTH     6
#define HIDC_INPUT_MOUSE_TOTAL (HIDC_INPUT_HEADER_LENGTH + HIDC_INPUT_MOUSE_BODY_LENGTH)

#define MAX_KEYBOARD_REPORT              6
#define HIDC_INPUT_KEYBOARD_BODY_LENGTH  (1 + 1 + 1 + MAX_KEYBOARD_REPORT)
#define HIDC_INPUT_KEYBOARD_TOTAL (HIDC_INPUT_HEADER_LENGTH + HIDC_INPUT_KEYBOARD_BODY_LENGTH)

#define HIDC_INPUT_MULTITOUCH_FINGERS 10
#define HIDC_INPUT_MULTITOUCH_BODY_LENGTH (1 + 6 * HIDC_INPUT_MULTITOUCH_FINGERS + 3)
#define HIDC_INPUT_MULTITOUCH_TOTAL (HIDC_INPUT_HEADER_LENGTH + HIDC_INPUT_MULTITOUCH_BODY_LENGTH)

#define HIDC_INPUT_MULTITOUCH_FEATURE_BODY_LENGTH (1 + 2)
#define HIDC_INPUT_MULTITOUCH_FEATURE_TOTAL (HIDC_INPUT_HEADER_LENGTH + HIDC_INPUT_MULTITOUCH_FEATURE_BODY_LENGTH)

#define LOGICAL_MAXIMUM 32767

// MultiTouch Descriptor
static unsigned char s_multiTouchDescriptor[] = {
    0x05, 0x0D,        // Usage Page (Digitizer)
    0x09, 0x04,        // Usage (Touch Screen)
    0xA1, 0x01,        // Collection (Application)
    0x85, 0x13,        //   Report ID (19)
    0x05, 0x0D,        //   Usage Page (Digitizer)
    0x09, 0x22,        //   Usage (Finger)
    0xA1, 0x02,        //   Collection (Logical)
    0x15, 0x00,        //     Logical Minimum (0)
    0x25, 0x01,        //     Logical Maximum (1)
    0x35, 0x00,        //     Physical Minimum (0)
    0x45, 0x00,        //     Physical Maximum (0)
    0x55, 0x00,        //     Unit Exponent (0)
    0x65, 0x00,        //     Unit (None)
    0x75, 0x01,        //     Report Size (1)
    0x95, 0x01,        //     Report Count (1)
    0x09, 0x42,        //     Usage (Tip Switch)
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x95, 0x07,        //     Report Count (7)
    0x81, 0x03,        //     Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x01,        //     Report Count (1)
    0x05, 0x0D,        //     Usage Page (Digitizer)
    0x09, 0x51,        //     Usage (0x51)
    0x26, 0x63, 0x00,  //     Logical Maximum (99)
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x75, 0x10,        //     Report Size (16)
    0x55, 0x0E,        //     Unit Exponent (-2)
    0x65, 0x11,        //     Unit (System: SI Linear, Length: Centimeter)
    0x27, 0xFF, 0x7F, 0x00, 0x00,  //     Logical Maximum (32766)
    0x46, 0x40, 0x06,  //     Physical Maximum (1600)
    0x05, 0x01,        //     Usage Page (Generic Desktop Ctrls)
    0x09, 0x30,        //     Usage (X)
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x46, 0x84, 0x03,  //     Physical Maximum (900)
    0x05, 0x01,        //     Usage Page (Generic Desktop Ctrls)
    0x09, 0x31,        //     Usage (Y)
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,              //   End Collection
    0x05, 0x0D,        //   Usage Page (Digitizer)
    0x09, 0x22,        //   Usage (Finger)
    0xA1, 0x02,        //   Collection (Logical)
    0x15, 0x00,        //     Logical Minimum (0)
    0x25, 0x01,        //     Logical Maximum (1)
    0x35, 0x00,        //     Physical Minimum (0)
    0x45, 0x00,        //     Physical Maximum (0)
    0x55, 0x00,        //     Unit Exponent (0)
    0x65, 0x00,        //     Unit (None)
    0x75, 0x01,        //     Report Size (1)
    0x95, 0x01,        //     Report Count (1)
    0x09, 0x42,        //     Usage (Tip Switch)
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x95, 0x07,        //     Report Count (7)
    0x81, 0x03,        //     Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x01,        //     Report Count (1)
    0x05, 0x0D,        //     Usage Page (Digitizer)
    0x09, 0x51,        //     Usage (0x51)
    0x26, 0x63, 0x00,  //     Logical Maximum (99)
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x75, 0x10,        //     Report Size (16)
    0x55, 0x0E,        //     Unit Exponent (-2)
    0x65, 0x11,        //     Unit (System: SI Linear, Length: Centimeter)
    0x27, 0xFF, 0x7F, 0x00, 0x00,  //     Logical Maximum (32766)
    0x46, 0x40, 0x06,  //     Physical Maximum (1600)
    0x05, 0x01,        //     Usage Page (Generic Desktop Ctrls)
    0x09, 0x30,        //     Usage (X)
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x46, 0x84, 0x03,  //     Physical Maximum (900)
    0x05, 0x01,        //     Usage Page (Generic Desktop Ctrls)
    0x09, 0x31,        //     Usage (Y)
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,              //   End Collection
    0x05, 0x0D,        //   Usage Page (Digitizer)
    0x09, 0x22,        //   Usage (Finger)
    0xA1, 0x02,        //   Collection (Logical)
    0x15, 0x00,        //     Logical Minimum (0)
    0x25, 0x01,        //     Logical Maximum (1)
    0x35, 0x00,        //     Physical Minimum (0)
    0x45, 0x00,        //     Physical Maximum (0)
    0x55, 0x00,        //     Unit Exponent (0)
    0x65, 0x00,        //     Unit (None)
    0x75, 0x01,        //     Report Size (1)
    0x95, 0x01,        //     Report Count (1)
    0x09, 0x42,        //     Usage (Tip Switch)
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x95, 0x07,        //     Report Count (7)
    0x81, 0x03,        //     Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x01,        //     Report Count (1)
    0x05, 0x0D,        //     Usage Page (Digitizer)
    0x09, 0x51,        //     Usage (0x51)
    0x26, 0x63, 0x00,  //     Logical Maximum (99)
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x75, 0x10,        //     Report Size (16)
    0x55, 0x0E,        //     Unit Exponent (-2)
    0x65, 0x11,        //     Unit (System: SI Linear, Length: Centimeter)
    0x27, 0xFF, 0x7F, 0x00, 0x00,  //     Logical Maximum (32766)
    0x46, 0x40, 0x06,  //     Physical Maximum (1600)
    0x05, 0x01,        //     Usage Page (Generic Desktop Ctrls)
    0x09, 0x30,        //     Usage (X)
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x46, 0x84, 0x03,  //     Physical Maximum (900)
    0x05, 0x01,        //     Usage Page (Generic Desktop Ctrls)
    0x09, 0x31,        //     Usage (Y)
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,              //   End Collection
    0x05, 0x0D,        //   Usage Page (Digitizer)
    0x09, 0x22,        //   Usage (Finger)
    0xA1, 0x02,        //   Collection (Logical)
    0x15, 0x00,        //     Logical Minimum (0)
    0x25, 0x01,        //     Logical Maximum (1)
    0x35, 0x00,        //     Physical Minimum (0)
    0x45, 0x00,        //     Physical Maximum (0)
    0x55, 0x00,        //     Unit Exponent (0)
    0x65, 0x00,        //     Unit (None)
    0x75, 0x01,        //     Report Size (1)
    0x95, 0x01,        //     Report Count (1)
    0x09, 0x42,        //     Usage (Tip Switch)
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x95, 0x07,        //     Report Count (7)
    0x81, 0x03,        //     Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x01,        //     Report Count (1)
    0x05, 0x0D,        //     Usage Page (Digitizer)
    0x09, 0x51,        //     Usage (0x51)
    0x26, 0x63, 0x00,  //     Logical Maximum (99)
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x75, 0x10,        //     Report Size (16)
    0x55, 0x0E,        //     Unit Exponent (-2)
    0x65, 0x11,        //     Unit (System: SI Linear, Length: Centimeter)
    0x27, 0xFF, 0x7F, 0x00, 0x00,  //     Logical Maximum (32766)
    0x46, 0x40, 0x06,  //     Physical Maximum (1600)
    0x05, 0x01,        //     Usage Page (Generic Desktop Ctrls)
    0x09, 0x30,        //     Usage (X)
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x46, 0x84, 0x03,  //     Physical Maximum (900)
    0x05, 0x01,        //     Usage Page (Generic Desktop Ctrls)
    0x09, 0x31,        //     Usage (Y)
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,              //   End Collection
    0x05, 0x0D,        //   Usage Page (Digitizer)
    0x09, 0x22,        //   Usage (Finger)
    0xA1, 0x02,        //   Collection (Logical)
    0x15, 0x00,        //     Logical Minimum (0)
    0x25, 0x01,        //     Logical Maximum (1)
    0x35, 0x00,        //     Physical Minimum (0)
    0x45, 0x00,        //     Physical Maximum (0)
    0x55, 0x00,        //     Unit Exponent (0)
    0x65, 0x00,        //     Unit (None)
    0x75, 0x01,        //     Report Size (1)
    0x95, 0x01,        //     Report Count (1)
    0x09, 0x42,        //     Usage (Tip Switch)
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x95, 0x07,        //     Report Count (7)
    0x81, 0x03,        //     Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x01,        //     Report Count (1)
    0x05, 0x0D,        //     Usage Page (Digitizer)
    0x09, 0x51,        //     Usage (0x51)
    0x26, 0x63, 0x00,  //     Logical Maximum (99)
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x75, 0x10,        //     Report Size (16)
    0x55, 0x0E,        //     Unit Exponent (-2)
    0x65, 0x11,        //     Unit (System: SI Linear, Length: Centimeter)
    0x27, 0xFF, 0x7F, 0x00, 0x00,  //     Logical Maximum (32766)
    0x46, 0x40, 0x06,  //     Physical Maximum (1600)
    0x05, 0x01,        //     Usage Page (Generic Desktop Ctrls)
    0x09, 0x30,        //     Usage (X)
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x46, 0x84, 0x03,  //     Physical Maximum (900)
    0x05, 0x01,        //     Usage Page (Generic Desktop Ctrls)
    0x09, 0x31,        //     Usage (Y)
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,              //   End Collection
    0x05, 0x0D,        //   Usage Page (Digitizer)
    0x09, 0x22,        //   Usage (Finger)
    0xA1, 0x02,        //   Collection (Logical)
    0x15, 0x00,        //     Logical Minimum (0)
    0x25, 0x01,        //     Logical Maximum (1)
    0x35, 0x00,        //     Physical Minimum (0)
    0x45, 0x00,        //     Physical Maximum (0)
    0x55, 0x00,        //     Unit Exponent (0)
    0x65, 0x00,        //     Unit (None)
    0x75, 0x01,        //     Report Size (1)
    0x95, 0x01,        //     Report Count (1)
    0x09, 0x42,        //     Usage (Tip Switch)
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x95, 0x07,        //     Report Count (7)
    0x81, 0x03,        //     Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x01,        //     Report Count (1)
    0x05, 0x0D,        //     Usage Page (Digitizer)
    0x09, 0x51,        //     Usage (0x51)
    0x26, 0x63, 0x00,  //     Logical Maximum (99)
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x75, 0x10,        //     Report Size (16)
    0x55, 0x0E,        //     Unit Exponent (-2)
    0x65, 0x11,        //     Unit (System: SI Linear, Length: Centimeter)
    0x27, 0xFF, 0x7F, 0x00, 0x00,  //     Logical Maximum (32766)
    0x46, 0x40, 0x06,  //     Physical Maximum (1600)
    0x05, 0x01,        //     Usage Page (Generic Desktop Ctrls)
    0x09, 0x30,        //     Usage (X)
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x46, 0x84, 0x03,  //     Physical Maximum (900)
    0x05, 0x01,        //     Usage Page (Generic Desktop Ctrls)
    0x09, 0x31,        //     Usage (Y)
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,              //   End Collection
    0x05, 0x0D,        //   Usage Page (Digitizer)
    0x09, 0x22,        //   Usage (Finger)
    0xA1, 0x02,        //   Collection (Logical)
    0x15, 0x00,        //     Logical Minimum (0)
    0x25, 0x01,        //     Logical Maximum (1)
    0x35, 0x00,        //     Physical Minimum (0)
    0x45, 0x00,        //     Physical Maximum (0)
    0x55, 0x00,        //     Unit Exponent (0)
    0x65, 0x00,        //     Unit (None)
    0x75, 0x01,        //     Report Size (1)
    0x95, 0x01,        //     Report Count (1)
    0x09, 0x42,        //     Usage (Tip Switch)
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x95, 0x07,        //     Report Count (7)
    0x81, 0x03,        //     Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x01,        //     Report Count (1)
    0x05, 0x0D,        //     Usage Page (Digitizer)
    0x09, 0x51,        //     Usage (0x51)
    0x26, 0x63, 0x00,  //     Logical Maximum (99)
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x75, 0x10,        //     Report Size (16)
    0x55, 0x0E,        //     Unit Exponent (-2)
    0x65, 0x11,        //     Unit (System: SI Linear, Length: Centimeter)
    0x27, 0xFF, 0x7F, 0x00, 0x00,  //     Logical Maximum (32766)
    0x46, 0x40, 0x06,  //     Physical Maximum (1600)
    0x05, 0x01,        //     Usage Page (Generic Desktop Ctrls)
    0x09, 0x30,        //     Usage (X)
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x46, 0x84, 0x03,  //     Physical Maximum (900)
    0x05, 0x01,        //     Usage Page (Generic Desktop Ctrls)
    0x09, 0x31,        //     Usage (Y)
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,              //   End Collection
    0x05, 0x0D,        //   Usage Page (Digitizer)
    0x09, 0x22,        //   Usage (Finger)
    0xA1, 0x02,        //   Collection (Logical)
    0x15, 0x00,        //     Logical Minimum (0)
    0x25, 0x01,        //     Logical Maximum (1)
    0x35, 0x00,        //     Physical Minimum (0)
    0x45, 0x00,        //     Physical Maximum (0)
    0x55, 0x00,        //     Unit Exponent (0)
    0x65, 0x00,        //     Unit (None)
    0x75, 0x01,        //     Report Size (1)
    0x95, 0x01,        //     Report Count (1)
    0x09, 0x42,        //     Usage (Tip Switch)
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x95, 0x07,        //     Report Count (7)
    0x81, 0x03,        //     Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x01,        //     Report Count (1)
    0x05, 0x0D,        //     Usage Page (Digitizer)
    0x09, 0x51,        //     Usage (0x51)
    0x26, 0x63, 0x00,  //     Logical Maximum (99)
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x75, 0x10,        //     Report Size (16)
    0x55, 0x0E,        //     Unit Exponent (-2)
    0x65, 0x11,        //     Unit (System: SI Linear, Length: Centimeter)
    0x27, 0xFF, 0x7F, 0x00, 0x00,  //     Logical Maximum (32766)
    0x46, 0x40, 0x06,  //     Physical Maximum (1600)
    0x05, 0x01,        //     Usage Page (Generic Desktop Ctrls)
    0x09, 0x30,        //     Usage (X)
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x46, 0x84, 0x03,  //     Physical Maximum (900)
    0x05, 0x01,        //     Usage Page (Generic Desktop Ctrls)
    0x09, 0x31,        //     Usage (Y)
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,              //   End Collection
    0x05, 0x0D,        //   Usage Page (Digitizer)
    0x09, 0x22,        //   Usage (Finger)
    0xA1, 0x02,        //   Collection (Logical)
    0x15, 0x00,        //     Logical Minimum (0)
    0x25, 0x01,        //     Logical Maximum (1)
    0x35, 0x00,        //     Physical Minimum (0)
    0x45, 0x00,        //     Physical Maximum (0)
    0x55, 0x00,        //     Unit Exponent (0)
    0x65, 0x00,        //     Unit (None)
    0x75, 0x01,        //     Report Size (1)
    0x95, 0x01,        //     Report Count (1)
    0x09, 0x42,        //     Usage (Tip Switch)
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x95, 0x07,        //     Report Count (7)
    0x81, 0x03,        //     Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x01,        //     Report Count (1)
    0x05, 0x0D,        //     Usage Page (Digitizer)
    0x09, 0x51,        //     Usage (0x51)
    0x26, 0x63, 0x00,  //     Logical Maximum (99)
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x75, 0x10,        //     Report Size (16)
    0x55, 0x0E,        //     Unit Exponent (-2)
    0x65, 0x11,        //     Unit (System: SI Linear, Length: Centimeter)
    0x27, 0xFF, 0x7F, 0x00, 0x00,  //     Logical Maximum (32766)
    0x46, 0x40, 0x06,  //     Physical Maximum (1600)
    0x05, 0x01,        //     Usage Page (Generic Desktop Ctrls)
    0x09, 0x30,        //     Usage (X)
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x46, 0x84, 0x03,  //     Physical Maximum (900)
    0x05, 0x01,        //     Usage Page (Generic Desktop Ctrls)
    0x09, 0x31,        //     Usage (Y)
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,              //   End Collection
    0x05, 0x0D,        //   Usage Page (Digitizer)
    0x09, 0x22,        //   Usage (Finger)
    0xA1, 0x02,        //   Collection (Logical)
    0x15, 0x00,        //     Logical Minimum (0)
    0x25, 0x01,        //     Logical Maximum (1)
    0x35, 0x00,        //     Physical Minimum (0)
    0x45, 0x00,        //     Physical Maximum (0)
    0x55, 0x00,        //     Unit Exponent (0)
    0x65, 0x00,        //     Unit (None)
    0x75, 0x01,        //     Report Size (1)
    0x95, 0x01,        //     Report Count (1)
    0x09, 0x42,        //     Usage (Tip Switch)
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x95, 0x07,        //     Report Count (7)
    0x81, 0x03,        //     Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x01,        //     Report Count (1)
    0x05, 0x0D,        //     Usage Page (Digitizer)
    0x09, 0x51,        //     Usage (0x51)
    0x26, 0x63, 0x00,  //     Logical Maximum (99)
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x75, 0x10,        //     Report Size (16)
    0x55, 0x0E,        //     Unit Exponent (-2)
    0x65, 0x11,        //     Unit (System: SI Linear, Length: Centimeter)
    0x27, 0xFF, 0x7F, 0x00, 0x00,  //     Logical Maximum (32766)
    0x46, 0x40, 0x06,  //     Physical Maximum (1600)
    0x05, 0x01,        //     Usage Page (Generic Desktop Ctrls)
    0x09, 0x30,        //     Usage (X)
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x46, 0x84, 0x03,  //     Physical Maximum (900)
    0x05, 0x01,        //     Usage Page (Generic Desktop Ctrls)
    0x09, 0x31,        //     Usage (Y)
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,              //   End Collection
    0x05, 0x0D,        //   Usage Page (Digitizer)
    0x09, 0x54,        //   Usage (0x54)
    0x95, 0x01,        //   Report Count (1)
    0x75, 0x08,        //   Report Size (8)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x63,        //   Logical Maximum (99)
    0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x55, 0x0C,        //   Unit Exponent (-4)
    0x66, 0x01, 0x10,  //   Unit (System: SI Linear, Time: Seconds)
    0x47, 0xFF, 0xFF, 0x00, 0x00,  //   Physical Maximum (65534)
    0x27, 0xFF, 0xFF, 0x00, 0x00,  //   Logical Maximum (65534)
    0x75, 0x10,        //   Report Size (16)
    0x95, 0x01,        //   Report Count (1)
    0x09, 0x56,        //   Usage (0x56)
    0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x05, 0x0D,        //   Usage Page (Digitizer)
    0x85, 0x12,        //   Report ID (18)
    0x09, 0x55,        //   Usage (0x55)
    0x95, 0x01,        //   Report Count (1)
    0x75, 0x10,        //   Report Size (16)
    0x15, 0x00,        //   Logical Minimum (0)
    0x26, 0x64, 0x00,  //   Logical Maximum (100)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0xC0               // End Collection
};

// Mouse Descriptor
static unsigned char s_mouseDescriptor[] = {
    0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
    0x09, 0x02,        // Usage (Mouse)
    0xA1, 0x01,        // Collection (Application)
    0x85, 0x28,        //   Report ID (40)
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

// Keyboard Descriptor
static unsigned char s_keyboardDescriptor[] = {
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

// UIBC Category
enum UIBCCategory
{
    UIBC_NotSupport = -1,
    UIBC_Generic = 0,
    UIBC_HIDC
};

enum SupportHIDTypeFlags {
    HIDTypeFlags_Keyboard    = 0x01,
    HIDTypeFlags_Mouse       = 0x02,
    HIDTypeFlags_SingleTouch = 0x04,
    HIDTypeFlags_MultiTouch  = 0x08,
    HIDTypeFlags_Joystick    = 0x10,
    HIDTypeFlags_Camera      = 0x20,
    HIDTypeFlags_Gesture     = 0x40,
    HIDTypeFlags_RemoteCtrl  = 0x80
};

enum GENERICMessageType {
    GENERIC_TOUCH_DOWN  = 0,
    GENERIC_TOUCH_UP    = 1,
    GENERIC_TOUCH_MOVE  = 2,
    GENERIC_KEY_DOWN    = 3,
    GENERIC_KEY_UP      = 4,
    GENERIC_ZOOM        = 5,
    GENERIC_VERTICAL_SCROLL     = 6,
    GENERIC_HORIZONTAL_SCROLL   = 7,
    GENERIC_ROTATE              = 8
};

class UIBCManager
{
public:
	UIBCManager();
	~UIBCManager();

    int ConnectUIBC(const std::string &ip, int port);
	bool Connected();
    void SetEnable(bool enable);
    bool GetEnable();

    void SetUIBCCategory(UIBCCategory category);
    UIBCCategory GetUIBCCategory();

    void SetHIDType(uint8_t typeFlags);
    uint8_t GetHIDType();

    bool SupportMultiTouch();

    // HIDC
    void SendHIDMouse(unsigned char type, char xdiff, char ydiff, char wdiff);
    void SendHIDKeyboard(unsigned char type, unsigned char modType, unsigned short keyboardValue);
    void SendHIDMultiTouch(const char* multiTouchMessage);
    void SendMultiTouchFeature();

    // Generic
    void SendGenericTouch(const char* inEventDesc, double widthRatio, double heightRatio);
    
    //TODO: not used, need test
    void SendGenericKey(const char* inEventDesc);
    void SendGenericZoom(const char* inEventDesc);
    void SendGenericScroll(const char* inEventDesc);
    void SendGenericRotate(const char* inEventDesc);

private:
	int m_uibcFd;
    bool m_uibcEnable;
	unsigned short m_pendingKeys[MAX_KEYBOARD_REPORT];
	bool m_shiftDown;
	bool m_altDown;
	bool m_ctrlDown;
    unsigned char m_lastButtons = 0;
    UIBCCategory m_uibcCategory;
    uint8_t m_HIDType;

#ifdef _WIN32
    // Windows-specific socket initialization and cleanup functions
    int InitWindowsSockets();
    void CleanupWindowsSockets();
#endif

    void SendHIDDescriptor(unsigned char type, unsigned char* body, unsigned short length);
    void SendUIBCEvent(unsigned char category, unsigned char* body, unsigned short length);
    // return HID usage ID
    char KeyboardValueToHID(unsigned short keyboardValue);
};

#endif // HH_UIBCMANAGER_H