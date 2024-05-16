#ifndef _SYS_KBD_H
#define _SYS_KBD_H 1

#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

#include "../_AUX/timespec.h"

typedef struct kbd_event
{
    struct timespec time;
    uint16_t type;
    uint16_t code;
} kbd_event_t;

#define KBD_EVENT_TYPE_PRESS 0
#define KBD_EVENT_TYPE_RELEASE 1

#define KEY_A 0
#define KEY_B 1
#define KEY_C 2
#define KEY_D 3
#define KEY_E 4
#define KEY_F 5
#define KEY_G 6
#define KEY_H 7
#define KEY_I 8
#define KEY_J 9
#define KEY_K 10
#define KEY_L 11
#define KEY_M 12
#define KEY_N 13
#define KEY_O 14
#define KEY_P 15
#define KEY_Q 16
#define KEY_R 17
#define KEY_S 18
#define KEY_T 19
#define KEY_U 20
#define KEY_V 21
#define KEY_W 22
#define KEY_X 23
#define KEY_Y 24
#define KEY_Z 25
#define KEY_1 26
#define KEY_2 27
#define KEY_3 28
#define KEY_4 29
#define KEY_5 30
#define KEY_6 31
#define KEY_7 32
#define KEY_8 33
#define KEY_9 34
#define KEY_0 35
#define KEY_TAB 36
#define KEY_SPACE 37
#define KEY_BACKSPACE 38
#define KEY_ENTER 39
#define KEY_EXCLAMATION_MARK 40
#define KEY_DOUBLE_QUOTE 41
#define KEY_NUMBER_SIGN 42
#define KEY_DOLLAR 43
#define KEY_PERCENT 44
#define KEY_AMPERSAND 45
#define KEY_APOSTROPHE 46
#define KEY_OPEN_PARENTHESIS 47
#define KEY_CLOSE_PARENTHESIS 48
#define KEY_ASTERISK 49
#define KEY_PLUS 50
#define KEY_COMMA 51
#define KEY_MINUS 52
#define KEY_PERIOD 53
#define KEY_SLASH 54
#define KEY_COLON 55
#define KEY_SEMICOLON 56
#define KEY_LESS_THAN 57
#define KEY_EQUAL 58
#define KEY_GREATER_THAN 59
#define KEY_QUESTION_MARK 60
#define KEY_AT 61
#define KEY_OPEN_BRACKET 62
#define KEY_CLOSE_BRACKET 63
#define KEY_BACKSLASH 64
#define KEY_CARET 65
#define KEY_UNDERSCORE 66
#define KEY_BACKTICK 67
#define KEY_OPEN_BRACE 68
#define KEY_CLOSE_BRACE 69
#define KEY_VERTICAL_BAR 70
#define KEY_TILDE 71
#define KEY_CAPS_LOCK 72
#define KEY_F1 73
#define KEY_F2 74
#define KEY_F3 75
#define KEY_F4 76
#define KEY_F5 77
#define KEY_F6 78
#define KEY_F7 79
#define KEY_F8 80
#define KEY_F9 81
#define KEY_F10 82
#define KEY_F11 83
#define KEY_F12 84
#define KEY_PRINT_SCREEN 85
#define KEY_SCROLL_LOCK 86
#define KEY_PAUSE 87
#define KEY_INSERT 88
#define KEY_HOME 89
#define KEY_DELETE 90
#define KEY_END 91
#define KEY_PAGE_UP 92
#define KEY_PAGE_DOWN 93
#define KEY_ARROW_UP 94
#define KEY_ARROW_DOWN 95
#define KEY_ARROW_LEFT 96
#define KEY_ARROW_RIGHT 97
#define KEY_NUM_LOCK 98
#define KEY_KEYPAD_EQUAL 99
#define KEY_KEYPAD_DIVIDE 100
#define KEY_KEYPAD_MULTIPLY 101
#define KEY_KEYPAD_MINUS 102
#define KEY_KEYPAD_PLUS 103
#define KEY_KEYPAD_ENTER 104
#define KEY_KEYPAD_1 105
#define KEY_KEYPAD_2 106
#define KEY_KEYPAD_3 107
#define KEY_KEYPAD_4 108
#define KEY_KEYPAD_5 109
#define KEY_KEYPAD_6 110
#define KEY_KEYPAD_7 111
#define KEY_KEYPAD_8 112
#define KEY_KEYPAD_9 113
#define KEY_KEYPAD_0 114
#define KEY_KEYPAD_PERIOD 115
#define KEY_LEFT_SHIFT 116
#define KEY_RIGHT_SHIFT 117
#define KEY_LEFT_CTRL 118
#define KEY_RIGHT_CTRL 119
#define KEY_LEFT_ALT 120
#define KEY_RIGHT_ALT 121
#define KEY_LEFT_SUPER 122
#define KEY_RIGHT_SUPER 123
#define KEY_ESC 124
#define KEY_SYSREQ 125
#define KEY_EUROPE_2 126
#define KEY_AMOUNT 127

#if defined(__cplusplus)
}
#endif

#endif