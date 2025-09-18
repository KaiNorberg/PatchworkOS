#ifndef _SYS_KBD_H
#define _SYS_KBD_H 1

#include <stdint.h>

#if defined(__cplusplus)
extern "C"
{
#endif

#include "_internal/clock_t.h"

/**
 * @brief Keyboard device header.
 * @ingroup libstd
 * @defgroup libstd_sys_kbd Keyboard device
 *
 * @{
 */

/**
 * @brief Keyboard keycode type.
 * @ingroup libstd_sys_kbd
 *
 */
typedef enum
{
    KBD_NONE = 0x00,          //!< None
    KBD_ERR_OVF = 0x01,       //!< Keyboard error overflow
    KBD_POST_FAIL = 0x02,     //!< POST failure
    KBD_ERR_UNDEFINED = 0x03, //!< Undefined error
    KBD_A = 0x04,             //!< Key A
    KBD_B = 0x05,             //!< Key B
    KBD_C = 0x06,             //!< Key C
    KBD_D = 0x07,             //!< Key D
    KBD_E = 0x08,             //!< Key E
    KBD_F = 0x09,             //!< Key F
    KBD_G = 0x0A,             //!< Key G
    KBD_H = 0x0B,             //!< Key H
    KBD_I = 0x0C,             //!< Key I
    KBD_J = 0x0D,             //!< Key J
    KBD_K = 0x0E,             //!< Key K
    KBD_L = 0x0F,             //!< Key L
    KBD_M = 0x10,             //!< Key M
    KBD_N = 0x11,             //!< Key N
    KBD_O = 0x12,             //!< Key O
    KBD_P = 0x13,             //!< Key P
    KBD_Q = 0x14,             //!< Key Q
    KBD_R = 0x15,             //!< Key R
    KBD_S = 0x16,             //!< Key S
    KBD_T = 0x17,             //!< Key T
    KBD_U = 0x18,             //!< Key U
    KBD_V = 0x19,             //!< Key V
    KBD_W = 0x1A,             //!< Key W
    KBD_X = 0x1B,             //!< Key X
    KBD_Y = 0x1C,             //!< Key Y
    KBD_Z = 0x1D,             //!< Key Z

    KBD_1 = 0x1E, //!< Key 1
    KBD_2 = 0x1F, //!< Key 2
    KBD_3 = 0x20, //!< Key 3
    KBD_4 = 0x21, //!< Key 4
    KBD_5 = 0x22, //!< Key 5
    KBD_6 = 0x23, //!< Key 6
    KBD_7 = 0x24, //!< Key 7
    KBD_8 = 0x25, //!< Key 8
    KBD_9 = 0x26, //!< Key 9
    KBD_0 = 0x27, //!< Key 0

    KBD_ENTER = 0x28,       //!< Enter key
    KBD_ESC = 0x29,         //!< Escape key
    KBD_BACKSPACE = 0x2A,   //!< Backspace key
    KBD_TAB = 0x2B,         //!< Tab key
    KBD_SPACE = 0x2C,       //!< Space key
    KBD_MINUS = 0x2D,       //!< Minus key
    KBD_EQUAL = 0x2E,       //!< Equal key
    KBD_LEFT_BRACE = 0x2F,  //!< Left brace key
    KBD_RIGHT_BRACE = 0x30, //!< Right brace key
    KBD_BACKSLASH = 0x31,   //!< Backslash key
    KBD_HASHTILDE = 0x32,   //!< Hashtilde key
    KBD_SEMICOLON = 0x33,   //!< Semicolon key
    KBD_APOSTROPHE = 0x34,  //!< Apostrophe key
    KBD_GRAVE = 0x35,       //!< Grave accent key
    KBD_COMMA = 0x36,       //!< Comma key
    KBD_PERIOD = 0x37,      //!< Period key
    KBD_SLASH = 0x38,       //!< Slash key
    KBD_CAPS_LOCK = 0x39,   //!< Caps Lock key

    KBD_F1 = 0x3A,  //!< F1 key
    KBD_F2 = 0x3B,  //!< F2 key
    KBD_F3 = 0x3C,  //!< F3 key
    KBD_F4 = 0x3D,  //!< F4 key
    KBD_F5 = 0x3E,  //!< F5 key
    KBD_F6 = 0x3F,  //!< F6 key
    KBD_F7 = 0x40,  //!< F7 key
    KBD_F8 = 0x41,  //!< F8 key
    KBD_F9 = 0x42,  //!< F9 key
    KBD_F10 = 0x43, //!< F10 key
    KBD_F11 = 0x44, //!< F11 key
    KBD_F12 = 0x45, //!< F12 key

    KBD_SYSRQ = 0x46,       //!< SysRq key
    KBD_SCROLL_LOCK = 0x47, //!< Scroll Lock key
    KBD_PAUSE = 0x48,       //!< Pause key
    KBD_INSERT = 0x49,      //!< Insert key
    KBD_HOME = 0x4A,        //!< Home key
    KBD_PAGE_UP = 0x4B,     //!< Page Up key
    KBD_DELETE = 0x4C,      //!< Delete key
    KBD_END = 0x4D,         //!< End key
    KBD_PAGE_DOWN = 0x4E,   //!< Page Down key
    KBD_RIGHT = 0x4F,       //!< Right arrow key
    KBD_LEFT = 0x50,        //!< Left arrow key
    KBD_DOWN = 0x51,        //!< Down arrow key
    KBD_UP = 0x52,          //!< Up arrow key

    KBD_NUM_LOCK = 0x53,    //!< Num Lock key
    KBD_KP_SLASH = 0x54,    //!< Keypad Slash
    KBD_KP_ASTERISK = 0x55, //!< Keypad Asterisk
    KBD_KP_MINUS = 0x56,    //!< Keypad Minus
    KBD_KP_PLUS = 0x57,     //!< Keypad Plus
    KBD_KP_ENTER = 0x58,    //!< Keypad Enter
    KBD_KP_1 = 0x59,        //!< Keypad 1
    KBD_KP_2 = 0x5A,        //!< Keypad 2
    KBD_KP_3 = 0x5B,        //!< Keypad 3
    KBD_KP_4 = 0x5C,        //!< Keypad 4
    KBD_KP_5 = 0x5D,        //!< Keypad 5
    KBD_KP_6 = 0x5E,        //!< Keypad 6
    KBD_KP_7 = 0x5F,        //!< Keypad 7
    KBD_KP_8 = 0x60,        //!< Keypad 8
    KBD_KP_9 = 0x61,        //!< Keypad 9
    KBD_KP_0 = 0x62,        //!< Keypad 0
    KBD_KP_PERIOD = 0x63,   //!< Keypad Period

    KBD_102ND = 0x64,    //!< 102nd key
    KBD_COMPOSE = 0x65,  //!< Compose key
    KBD_POWER = 0x66,    //!< Power key
    KBD_KP_EQUAL = 0x67, //!< Keypad Equal

    KBD_F13 = 0x68, //!< F13 key
    KBD_F14 = 0x69, //!< F14 key
    KBD_F15 = 0x6A, //!< F15 key
    KBD_F16 = 0x6B, //!< F16 key
    KBD_F17 = 0x6C, //!< F17 key
    KBD_F18 = 0x6D, //!< F18 key
    KBD_F19 = 0x6E, //!< F19 key
    KBD_F20 = 0x6F, //!< F20 key
    KBD_F21 = 0x70, //!< F21 key
    KBD_F22 = 0x71, //!< F22 key
    KBD_F23 = 0x72, //!< F23 key
    KBD_F24 = 0x73, //!< F24 key

    KBD_OPEN = 0x74,                //!< Open key
    KBD_HELP = 0x75,                //!< Help key
    KBD_PROPS = 0x76,               //!< Props key
    KBD_FRONT = 0x77,               //!< Front key
    KBD_STOP = 0x78,                //!< Stop key
    KBD_AGAIN = 0x79,               //!< Again key
    KBD_UNDO = 0x7A,                //!< Undo key
    KBD_CUT = 0x7B,                 //!< Cut key
    KBD_COPY = 0x7C,                //!< Copy key
    KBD_PASTE = 0x7D,               //!< Paste key
    KBD_FIND = 0x7E,                //!< Find key
    KBD_MUTE = 0x7F,                //!< Mute key
    KBD_VOLUME_UP = 0x80,           //!< Volume Up key
    KBD_VOLUME_DOWN = 0x81,         //!< Volume Down key
    KBD_LOCKING_CAPS_LOCK = 0x82,   //!< Locking Caps Lock key
    KBD_LOCKING_NUM_LOCK = 0x83,    //!< Locking Num Lock key
    KBD_LOCKING_SCROLL_LOCK = 0x84, //!< Locking Scroll Lock key
    KBD_KP_COMMA = 0x85,            //!< Keypad Comma
    KBD_KP_EQUAL_SIGN = 0x86,       //!< Keypad Equal Sign
    KBD_RO = 0x87,                  //!< RO key
    KBD_KATAKANAHIRAGANA = 0x88,    //!< Katakana/Hiragana key
    KBD_YEN = 0x89,                 //!< Yen key
    KBD_HENKAN = 0x8A,              //!< Henkan key
    KBD_MUHENKAN = 0x8B,            //!< Muhenkan key
    KBD_KP_JPCOMMA = 0x8C,          //!< Keypad Japanese Comma
    KBD_INTERNATIONAL7 = 0x8D,      //!< International 7 key
    KBD_INTERNATIONAL8 = 0x8E,      //!< International 8 key
    KBD_INTERNATIONAL9 = 0x8F,      //!< International 9 key
    KBD_HANGEUL = 0x90,             //!< Hangeul key
    KBD_HANJA = 0x91,               //!< Hanja key
    KBD_KATAKANA = 0x92,            //!< Katakana key
    KBD_HIRAGANA = 0x93,            //!< Hiragana key
    KBD_ZENKAKUHANKAKU = 0x94,      //!< Zenkaku/Hankaku key
    KBD_LANG6 = 0x95,               //!< Language 6 key
    KBD_LANG7 = 0x96,               //!< Language 7 key
    KBD_LANG8 = 0x97,               //!< Language 8 key
    KBD_LANG9 = 0x98,               //!< Language 9 key
    KBD_ALTERNATE_ERASE = 0x99,     //!< Alternate Erase key
    KBD_SYSREQ = 0x9A,              //!< SysReq key
    KBD_CANCEL = 0x9B,              //!< Cancel key
    KBD_CLEAR = 0x9C,               //!< Clear key
    KBD_PRIOR = 0x9D,               //!< Prior key
    KBD_RETURN = 0x9E,              //!< Return key
    KBD_SEPARATOR = 0x9F,           //!< Separator key
    KBD_OUT = 0xA0,                 //!< Out key
    KBD_OPER = 0xA1,                //!< Oper key
    KBD_CLEAR_AGAIN = 0xA2,         //!< Clear Again key
    KBD_CRSEL_PROPS = 0xA3,         //!< CrSel/Props key
    KBD_EXSEL = 0xA4,               //!< ExSel key

    KBD_KP_00 = 0xB0,               //!< Keypad 00
    KBD_KP_000 = 0xB1,              //!< Keypad 000
    KBD_THOUSANDS_SEPARATOR = 0xB2, //!< Thousands Separator key
    KBD_DECIMAL_SEPARATOR = 0xB3,   //!< Decimal Separator key
    KBD_CURRENCY_UNIT = 0xB4,       //!< Currency Unit key
    KBD_CURRENCY_SUBUNIT = 0xB5,    //!< Currency Subunit key
    KBD_KP_LEFT_PAREN = 0xB6,       //!< Keypad Left Parenthesis
    KBD_KP_RIGHT_PAREN = 0xB7,      //!< Keypad Right Parenthesis
    KBD_KP_LEFT_BRACE = 0xB8,       //!< Keypad Left Brace
    KBD_KP_RIGHT_BRACE = 0xB9,      //!< Keypad Right Brace
    KBD_KP_TAB = 0xBA,              //!< Keypad Tab
    KBD_KP_BACKSPACE = 0xBB,        //!< Keypad Backspace
    KBD_KP_A = 0xBC,                //!< Keypad A
    KBD_KP_B = 0xBD,                //!< Keypad B
    KBD_KP_C = 0xBE,                //!< Keypad C
    KBD_KP_D = 0xBF,                //!< Keypad D
    KBD_KP_E = 0xC0,                //!< Keypad E
    KBD_KP_F = 0xC1,                //!< Keypad F
    KBD_KP_XOR = 0xC2,              //!< Keypad XOR
    KBD_KP_CARET = 0xC3,            //!< Keypad Caret
    KBD_KP_PERCENT = 0xC4,          //!< Keypad Percent
    KBD_KP_LESS = 0xC5,             //!< Keypad Less
    KBD_KP_GREATER = 0xC6,          //!< Keypad Greater
    KBD_KP_AMPERSAND = 0xC7,        //!< Keypad Ampersand
    KBD_KP_DOUBLE_AMPERSAND = 0xC8, //!< Keypad Double Ampersand
    KBD_KP_PIPE = 0xC9,             //!< Keypad Pipe
    KBD_KP_DOUBLE_PIPE = 0xCA,      //!< Keypad Double Pipe
    KBD_KP_COLON = 0xCB,            //!< Keypad Colon
    KBD_KP_HASH = 0xCC,             //!< Keypad Hash
    KBD_KP_SPACE = 0xCD,            //!< Keypad Space
    KBD_KP_AT = 0xCE,               //!< Keypad At
    KBD_KP_EXCLAMATION = 0xCF,      //!< Keypad Exclamation
    KBD_KP_MEMORY_STORE = 0xD0,     //!< Keypad Memory Store
    KBD_KP_MEMORY_RECALL = 0xD1,    //!< Keypad Memory Recall
    KBD_KP_MEMORY_CLEAR = 0xD2,     //!< Keypad Memory Clear
    KBD_KP_MEMORY_ADD = 0xD3,       //!< Keypad Memory Add
    KBD_KP_MEMORY_SUBTRACT = 0xD4,  //!< Keypad Memory Subtract
    KBD_KP_MEMORY_MULTIPLY = 0xD5,  //!< Keypad Memory Multiply
    KBD_KP_MEMORY_DIVIDE = 0xD6,    //!< Keypad Memory Divide
    KBD_KP_PLUS_MINUS = 0xD7,       //!< Keypad Plus/Minus
    KBD_KP_CLEAR = 0xD8,            //!< Keypad Clear
    KBD_KP_CLEAR_ENTRY = 0xD9,      //!< Keypad Clear Entry
    KBD_KP_BINARY = 0xDA,           //!< Keypad Binary
    KBD_KP_OCTAL = 0xDB,            //!< Keypad Octal
    KBD_KP_DECIMAL = 0xDC,          //!< Keypad Decimal
    KBD_KP_HEXADECIMAL = 0xDD,      //!< Keypad Hexadecimal

    KBD_LEFT_CTRL = 0xE0,   //!< Left Control key
    KBD_LEFT_SHIFT = 0xE1,  //!< Left Shift key
    KBD_LEFT_ALT = 0xE2,    //!< Left Alt key
    KBD_LEFT_SUPER = 0xE3,  //!< Left Super key
    KBD_RIGHT_CTRL = 0xE4,  //!< Right Control key
    KBD_RIGHT_SHIFT = 0xE5, //!< Right Shift key
    KBD_RIGHT_ALT = 0xE6,   //!< Right Alt key
    KBD_RIGHT_SUPER = 0xE7, //!< Right Super key

    KBD_MEDIA_PLAY_PAUSE = 0xE8,    //!< Media Play/Pause key
    KBD_MEDIA_STOP_CD = 0xE9,       //!< Media Stop CD key
    KBD_MEDIA_PREVIOUS_SONG = 0xEA, //!< Media Previous Song key
    KBD_MEDIA_NEXT_SONG = 0xEB,     //!< Media Next Song key
    KBD_MEDIA_EJECT_CD = 0xEC,      //!< Media Eject CD key
    KBD_MEDIA_VOLUME_UP = 0xED,     //!< Media Volume Up key
    KBD_MEDIA_VOLUME_DOWN = 0xEE,   //!< Media Volume Down key
    KBD_MEDIA_MUTE = 0xEF,          //!< Media Mute key
    KBD_MEDIA_WWW = 0xF0,           //!< Media WWW key
    KBD_MEDIA_BACK = 0xF1,          //!< Media Back key
    KBD_MEDIA_FORWARD = 0xF2,       //!< Media Forward key
    KBD_MEDIA_STOP = 0xF3,          //!< Media Stop key
    KBD_MEDIA_FIND = 0xF4,          //!< Media Find key
    KBD_MEDIA_SCROLL_UP = 0xF5,     //!< Media Scroll Up key
    KBD_MEDIA_SCROLL_DOWN = 0xF6,   //!< Media Scroll Down key
    KBD_MEDIA_EDIT = 0xF7,          //!< Media Edit key
    KBD_MEDIA_SLEEP = 0xF8,         //!< Media Sleep key
    KBD_MEDIA_COFFEE = 0xF9,        //!< Media Coffee key
    KBD_MEDIA_REFRESH = 0xFA,       //!< Media Refresh key
    KBD_MEDIA_CALC = 0xFB,          //!< Media Calculator key
} keycode_t;

/**
 * @brief Keyboard event type.
 * @ingroup libstd_sys_kbd
 *
 */
typedef enum
{
    KBD_PRESS = 0,  //!< Key press event
    KBD_RELEASE = 1 //!< Key release event
} kbd_event_type_t;

/**
 * @brief Keyboard modifiers type.
 * @ingroup libstd_sys_kbd
 *
 */
typedef enum
{
    KBD_MOD_NONE = 0,       //!< No modifier
    KBD_MOD_CAPS = 1 << 0,  //!< Caps Lock modifier
    KBD_MOD_SHIFT = 1 << 1, //!< Shift modifier
    KBD_MOD_CTRL = 1 << 2,  //!< Control modifier
    KBD_MOD_ALT = 1 << 3,   //!< Alt modifier
    KBD_MOD_SUPER = 1 << 4, //!< Super (Windows/Command) modifier
} kbd_mods_t;

/**
 * @brief Keyboard event structure.
 * @ingroup libstd_sys_kbd
 *
 * The `kbd_event_t` structure read from keyboard files, for example `/dev/kbd/ps2`. Keyboard files will block
 * until a key event happens, keyboard files will never return partial events.
 */
typedef struct
{
    clock_t time;          //!< Timestamp of the event
    kbd_event_type_t type; //!< Type of keyboard event (press or release)
    kbd_mods_t mods;       //!< Active keyboard modifiers
    keycode_t code;        //!< Keycode of the key involved in the event
} kbd_event_t;

#if defined(__cplusplus)
}
#endif

#endif

/** @} */
