#ifndef _SYS_KBD_H
#define _SYS_KBD_H 1

#include <stdint.h>

#if defined(__cplusplus)
extern "C"
{
#endif

#include "_AUX/nsec_t.h"

typedef enum kbd_event_type
{
    KBD_PRESS = 0,
    KBD_RELEASE = 1
} kbd_event_type_t;

typedef enum key_code
{
    KEY_A = 0,
    KEY_B = 1,
    KEY_C = 2,
    KEY_D = 3,
    KEY_E = 4,
    KEY_F = 5,
    KEY_G = 6,
    KEY_H = 7,
    KEY_I = 8,
    KEY_J = 9,
    KEY_K = 10,
    KEY_L = 11,
    KEY_M = 12,
    KEY_N = 13,
    KEY_O = 14,
    KEY_P = 15,
    KEY_Q = 16,
    KEY_R = 17,
    KEY_S = 18,
    KEY_T = 19,
    KEY_U = 20,
    KEY_V = 21,
    KEY_W = 22,
    KEY_X = 23,
    KEY_Y = 24,
    KEY_Z = 25,
    KEY_1 = 26,
    KEY_2 = 27,
    KEY_3 = 28,
    KEY_4 = 29,
    KEY_5 = 30,
    KEY_6 = 31,
    KEY_7 = 32,
    KEY_8 = 33,
    KEY_9 = 34,
    KEY_0 = 35,
    KEY_TAB = 36,
    KEY_SPACE = 37,
    KEY_BACKSPACE = 38,
    KEY_ENTER = 39,
    KEY_EXCLAMATION_MARK = 40,
    KEY_DOUBLE_QUOTE = 41,
    KEY_NUMBER_SIGN = 42,
    KEY_DOLLAR = 43,
    KEY_PERCENT = 44,
    KEY_AMPERSAND = 45,
    KEY_APOSTROPHE = 46,
    KEY_OPEN_PARENTHESIS = 47,
    KEY_CLOSE_PARENTHESIS = 48,
    KEY_ASTERISK = 49,
    KEY_PLUS = 50,
    KEY_COMMA = 51,
    KEY_MINUS = 52,
    KEY_PERIOD = 53,
    KEY_SLASH = 54,
    KEY_COLON = 55,
    KEY_SEMICOLON = 56,
    KEY_LESS_THAN = 57,
    KEY_EQUAL = 58,
    KEY_GREATER_THAN = 59,
    KEY_QUESTION_MARK = 60,
    KEY_AT = 61,
    KEY_OPEN_BRACKET = 62,
    KEY_CLOSE_BRACKET = 63,
    KEY_BACKSLASH = 64,
    KEY_CARET = 65,
    KEY_UNDERSCORE = 66,
    KEY_BACKTICK = 67,
    KEY_OPEN_BRACE = 68,
    KEY_CLOSE_BRACE = 69,
    KEY_VERTICAL_BAR = 70,
    KEY_TILDE = 71,
    KEY_CAPS_LOCK = 72,
    KEY_F1 = 73,
    KEY_F2 = 74,
    KEY_F3 = 75,
    KEY_F4 = 76,
    KEY_F5 = 77,
    KEY_F6 = 78,
    KEY_F7 = 79,
    KEY_F8 = 80,
    KEY_F9 = 81,
    KEY_F10 = 82,
    KEY_F11 = 83,
    KEY_F12 = 84,
    KEY_PRINT_SCREEN = 85,
    KEY_SCROLL_LOCK = 86,
    KEY_PAUSE = 87,
    KEY_INSERT = 88,
    KEY_HOME = 89,
    KEY_DELETE = 90,
    KEY_END = 91,
    KEY_PAGE_UP = 92,
    KEY_PAGE_DOWN = 93,
    KEY_ARROW_UP = 94,
    KEY_ARROW_DOWN = 95,
    KEY_ARROW_LEFT = 96,
    KEY_ARROW_RIGHT = 97,
    KEY_NUM_LOCK = 98,
    KEY_KEYPAD_EQUAL = 99,
    KEY_KEYPAD_DIVIDE = 100,
    KEY_KEYPAD_MULTIPLY = 101,
    KEY_KEYPAD_MINUS = 102,
    KEY_KEYPAD_PLUS = 103,
    KEY_KEYPAD_ENTER = 104,
    KEY_KEYPAD_1 = 105,
    KEY_KEYPAD_2 = 106,
    KEY_KEYPAD_3 = 107,
    KEY_KEYPAD_4 = 108,
    KEY_KEYPAD_5 = 109,
    KEY_KEYPAD_6 = 110,
    KEY_KEYPAD_7 = 111,
    KEY_KEYPAD_8 = 112,
    KEY_KEYPAD_9 = 113,
    KEY_KEYPAD_0 = 114,
    KEY_KEYPAD_PERIOD = 115,
    KEY_LEFT_SHIFT = 116,
    KEY_RIGHT_SHIFT = 117,
    KEY_LEFT_CTRL = 118,
    KEY_RIGHT_CTRL = 119,
    KEY_LEFT_ALT = 120,
    KEY_RIGHT_ALT = 121,
    KEY_LEFT_SUPER = 122,
    KEY_RIGHT_SUPER = 123,
    KEY_ESC = 124,
    KEY_SYSREQ = 125,
    KEY_EUROPE_2 = 126,
    KEY_AMOUNT = 127
} key_code_t;

typedef struct kbd_event
{
    nsec_t time;
    kbd_event_type_t type;
    key_code_t code;
} kbd_event_t;

#if defined(__cplusplus)
}
#endif

#endif
