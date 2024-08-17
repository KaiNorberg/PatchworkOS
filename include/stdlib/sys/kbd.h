#ifndef _SYS_KBD_H
#define _SYS_KBD_H 1

#include <stdint.h>

#if defined(__cplusplus)
extern "C"
{
#endif

#include "_AUX/nsec_t.h"

typedef enum keycode
{
    KEY_NONE = 0x00,          // No key pressed
    KEY_ERR_OVF = 0x01,       // Keyboard Error Roll Over - used for all slots if too many keys are pressed ("Phantom key")
    KEY_POST_FAIL = 0x02,     // Keyboard POST Fail
    KEY_ERR_UNDEFINED = 0x03, // Keyboard Error Undefined
    KEY_A = 0x04,             // Keyboard a and A
    KEY_B = 0x05,             // Keyboard b and B
    KEY_C = 0x06,             // Keyboard c and C
    KEY_D = 0x07,             // Keyboard d and D
    KEY_E = 0x08,             // Keyboard e and E
    KEY_F = 0x09,             // Keyboard f and F
    KEY_G = 0x0A,             // Keyboard g and G
    KEY_H = 0x0B,             // Keyboard h and H
    KEY_I = 0x0C,             // Keyboard i and I
    KEY_J = 0x0D,             // Keyboard j and J
    KEY_K = 0x0E,             // Keyboard k and K
    KEY_L = 0x0F,             // Keyboard l and L
    KEY_M = 0x10,             // Keyboard m and M
    KEY_N = 0x11,             // Keyboard n and N
    KEY_O = 0x12,             // Keyboard o and O
    KEY_P = 0x13,             // Keyboard p and P
    KEY_Q = 0x14,             // Keyboard q and Q
    KEY_R = 0x15,             // Keyboard r and R
    KEY_S = 0x16,             // Keyboard s and S
    KEY_T = 0x17,             // Keyboard t and T
    KEY_U = 0x18,             // Keyboard u and U
    KEY_V = 0x19,             // Keyboard v and V
    KEY_W = 0x1A,             // Keyboard w and W
    KEY_X = 0x1B,             // Keyboard x and X
    KEY_Y = 0x1C,             // Keyboard y and Y
    KEY_Z = 0x1D,             // Keyboard z and Z

    KEY_1 = 0x1E, // Keyboard 1 and !
    KEY_2 = 0x1F, // Keyboard 2 and @
    KEY_3 = 0x20, // Keyboard 3 and #
    KEY_4 = 0x21, // Keyboard 4 and $
    KEY_5 = 0x22, // Keyboard 5 and %
    KEY_6 = 0x23, // Keyboard 6 and ^
    KEY_7 = 0x24, // Keyboard 7 and &
    KEY_8 = 0x25, // Keyboard 8 and *
    KEY_9 = 0x26, // Keyboard 9 and (
    KEY_0 = 0x27, // Keyboard 0 and )

    KEY_ENTER = 0x28,       // Keyboard Return (ENTER)
    KEY_ESC = 0x29,         // Keyboard ESCAPE
    KEY_BACKSPACE = 0x2A,   // Keyboard DELETE (Backspace)
    KEY_TAB = 0x2B,         // Keyboard Tab
    KEY_SPACE = 0x2C,       // Keyboard Spacebar
    KEY_MINUS = 0x2D,       // Keyboard - and _
    KEY_EQUAL = 0x2E,       // Keyboard = and +
    KEY_LEFT_BRACE = 0x2F,  // Keyboard [ and {
    KEY_RIGHT_BRACE = 0x30, // Keyboard ] and }
    KEY_BACKSLASH = 0x31,   // Keyboard \ and |
    KEY_HASHTILDE = 0x32,   // Keyboard Non-US # and ~
    KEY_SEMICOLON = 0x33,   // Keyboard ; and :
    KEY_APOSTROPHE = 0x34,  // Keyboard ' and "
    KEY_GRAVE = 0x35,       // Keyboard ` and ~
    KEY_COMMA = 0x36,       // Keyboard , and <
    KEY_PERIOD = 0x37,      // Keyboard . and >
    KEY_SLASH = 0x38,       // Keyboard / and ?
    KEY_CAPS_LOCK = 0x39,   // Keyboard Caps Lock

    KEY_F1 = 0x3A,  // Keyboard F1
    KEY_F2 = 0x3B,  // Keyboard F2
    KEY_F3 = 0x3C,  // Keyboard F3
    KEY_F4 = 0x3D,  // Keyboard F4
    KEY_F5 = 0x3E,  // Keyboard F5
    KEY_F6 = 0x3F,  // Keyboard F6
    KEY_F7 = 0x40,  // Keyboard F7
    KEY_F8 = 0x41,  // Keyboard F8
    KEY_F9 = 0x42,  // Keyboard F9
    KEY_F10 = 0x43, // Keyboard F10
    KEY_F11 = 0x44, // Keyboard F11
    KEY_F12 = 0x45, // Keyboard F12

    KEY_SYSRQ = 0x46,       // Keyboard Print Screen
    KEY_SCROLL_LOCK = 0x47, // Keyboard Scroll Lock
    KEY_PAUSE = 0x48,       // Keyboard Pause
    KEY_INSERT = 0x49,      // Keyboard Insert
    KEY_HOME = 0x4A,        // Keyboard Home
    KEY_PAGE_UP = 0x4B,     // Keyboard Page Up
    KEY_DELETE = 0x4C,      // Keyboard Delete Forward
    KEY_END = 0x4D,         // Keyboard End
    KEY_PAGE_DOWN = 0x4E,   // Keyboard Page Down
    KEY_RIGHT = 0x4F,       // Keyboard Right Arrow
    KEY_LEFT = 0x50,        // Keyboard Left Arrow
    KEY_DOWN = 0x51,        // Keyboard Down Arrow
    KEY_UP = 0x52,          // Keyboard Up Arrow

    KEY_NUM_LOCK = 0x53,    // Keyboard Num Lock and Clear
    KEY_KP_SLASH = 0x54,    // Keypad /
    KEY_KP_ASTERISK = 0x55, // Keypad *
    KEY_KP_MINUS = 0x56,    // Keypad -
    KEY_KP_PLUS = 0x57,     // Keypad +
    KEY_KP_ENTER = 0x58,    // Keypad ENTER
    KEY_KP_1 = 0x59,        // Keypad 1 and End
    KEY_KP_2 = 0x5A,        // Keypad 2 and Down Arrow
    KEY_KP_3 = 0x5B,        // Keypad 3 and PageDn
    KEY_KP_4 = 0x5C,        // Keypad 4 and Left Arrow
    KEY_KP_5 = 0x5D,        // Keypad 5
    KEY_KP_6 = 0x5E,        // Keypad 6 and Right Arrow
    KEY_KP_7 = 0x5F,        // Keypad 7 and Home
    KEY_KP_8 = 0x60,        // Keypad 8 and Up Arrow
    KEY_KP_9 = 0x61,        // Keypad 9 and Page Up
    KEY_KP_0 = 0x62,        // Keypad 0 and Insert
    KEY_KP_PERIOD = 0x63,   // Keypad . and Delete

    KEY_102ND = 0x64,    // Keyboard Non-US \ and |
    KEY_COMPOSE = 0x65,  // Keyboard Application
    KEY_POWER = 0x66,    // Keyboard Power
    KEY_KP_EQUAL = 0x67, // Keypad =

    KEY_F13 = 0x68, // Keyboard F13
    KEY_F14 = 0x69, // Keyboard F14
    KEY_F15 = 0x6A, // Keyboard F15
    KEY_F16 = 0x6B, // Keyboard F16
    KEY_F17 = 0x6C, // Keyboard F17
    KEY_F18 = 0x6D, // Keyboard F18
    KEY_F19 = 0x6E, // Keyboard F19
    KEY_F20 = 0x6F, // Keyboard F20
    KEY_F21 = 0x70, // Keyboard F21
    KEY_F22 = 0x71, // Keyboard F22
    KEY_F23 = 0x72, // Keyboard F23
    KEY_F24 = 0x73, // Keyboard F24

    KEY_OPEN = 0x74,        // Keyboard Execute
    KEY_HELP = 0x75,        // Keyboard Help
    KEY_PROPS = 0x76,       // Keyboard Menu
    KEY_FRONT = 0x77,       // Keyboard Select
    KEY_STOP = 0x78,        // Keyboard Stop
    KEY_AGAIN = 0x79,       // Keyboard Again
    KEY_UNDO = 0x7A,        // Keyboard Undo
    KEY_CUT = 0x7B,         // Keyboard Cut
    KEY_COPY = 0x7C,        // Keyboard Copy
    KEY_PASTE = 0x7D,       // Keyboard Paste
    KEY_FIND = 0x7E,        // Keyboard Find
    KEY_MUTE = 0x7F,        // Keyboard Mute
    KEY_VOLUME_UP = 0x80,   // Keyboard Volume Up
    KEY_VOLUME_DOWN = 0x81, // Keyboard Volume Down

    KEY_KP_COMMA = 0x85, // Keypad Comma

    KEY_RO = 0x87,               // Keyboard International1
    KEY_KATAKANAHIRAGANA = 0x88, // Keyboard International2
    KEY_YEN = 0x89,              // Keyboard International3
    KEY_HENKAN = 0x8A,           // Keyboard International4
    KEY_MUHENKAN = 0x8B,         // Keyboard International5
    KEY_KP_JPCOMMA = 0x8C,       // Keyboard International6

    KEY_HANGEUL = 0x90,        // Keyboard LANG1
    KEY_HANJA = 0x91,          // Keyboard LANG2
    KEY_KATAKANA = 0x92,       // Keyboard LANG3
    KEY_HIRAGANA = 0x93,       // Keyboard LANG4
    KEY_ZENKAKUHANKAKU = 0x94, // Keyboard LANG5

    KEY_SYSREQ = 0x9A,

    KEY_KP_LEFT_PAREN = 0xB6,  // Keypad (
    KEY_KP_RIGHT_PAREN = 0xB7, // Keypad )

    KEY_LEFT_CTRL = 0xE0,   // Keyboard Left Control
    KEY_LEFT_SHIFT = 0xE1,  // Keyboard Left Shift
    KEY_LEFT_ALT = 0xE2,    // Keyboard Left Alt
    KEY_LEFT_SUPER = 0xE3,  // Keyboard Left GUI
    KEY_RIGHT_CTRL = 0xE4,  // Keyboard Right Control
    KEY_RIGHT_SHIFT = 0xE5, // Keyboard Right Shift
    KEY_RIGHT_ALT = 0xE6,   // Keyboard Right Alt
    KEY_RIGHT_SUPER = 0xE7, // Keyboard Right GUI

    KEY_MEDIA_PLAY_PAUSE = 0xE8,
    KEY_MEDIA_STOP_CD = 0xE9,
    KEY_MEDIA_PREVIOUS_SONG = 0xEA,
    KEY_MEDIA_NEXT_SONG = 0xEB,
    KEY_MEDIA_EJECT_CD = 0xEC,
    KEY_MEDIA_VOLUME_UP = 0xED,
    KEY_MEDIA_VOLUME_DOWN = 0xEE,
    KEY_MEDIA_MUTE = 0xEF,
    KEY_MEDIA_WWW = 0xF0,
    KEY_MEDIA_BACK = 0xF1,
    KEY_MEDIA_FORWARD = 0xF2,
    KEY_MEDIA_STOP = 0xF3,
    KEY_MEDIA_FIND = 0xF4,
    KEY_MEDIA_SCROLL_UP = 0xF5,
    KEY_MEDIA_SCROLL_DOWN = 0xF6,
    KEY_MEDIA_EDIT = 0xF7,
    KEY_MEDIA_SLEEP = 0xF8,
    KEY_MEDIA_COFFEE = 0xF9,
    KEY_MEDIA_REFRESH = 0xFA,
    KEY_MEDIA_CALC = 0xFB
} keycode_t;

typedef enum kbd_event_type
{
    KBD_PRESS = 0,
    KBD_RELEASE = 1
} kbd_event_type_t;

typedef enum kbd_mods
{
    KBD_MOD_NONE = 0,
    KBD_MOD_CAPS = 1 << 0,
    KBD_MOD_SHIFT = 1 << 1,
    KBD_MOD_CTRL = 1 << 2,
    KBD_MOD_ALT = 1 << 3,
    KBD_MOD_SUPER = 1 << 4,
} kbd_mods_t;

typedef struct kbd_event
{
    nsec_t time;
    kbd_event_type_t type;
    kbd_mods_t mods;
    keycode_t code;
} kbd_event_t;

char kbd_ascii(keycode_t code, kbd_mods_t mods);

#if defined(__cplusplus)
}
#endif

#endif
