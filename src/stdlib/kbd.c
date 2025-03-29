#include <stdint.h>
#include <sys/kbd.h>

// TODO: Create a system for keymap files

typedef struct
{
    char norm;
    char shift;
} keymap_entry_t;

typedef struct
{
    keymap_entry_t map[UINT8_MAX];
} keymap_t;

static keymap_t keymap =
    {
        .map =
            {
                [KEY_A] = {.norm = 'a', .shift = 'A'},
                [KEY_B] = {.norm = 'b', .shift = 'B'},
                [KEY_C] = {.norm = 'c', .shift = 'C'},
                [KEY_D] = {.norm = 'd', .shift = 'D'},
                [KEY_E] = {.norm = 'e', .shift = 'E'},
                [KEY_F] = {.norm = 'f', .shift = 'F'},
                [KEY_G] = {.norm = 'g', .shift = 'G'},
                [KEY_H] = {.norm = 'h', .shift = 'H'},
                [KEY_I] = {.norm = 'i', .shift = 'I'},
                [KEY_J] = {.norm = 'j', .shift = 'J'},
                [KEY_K] = {.norm = 'k', .shift = 'K'},
                [KEY_L] = {.norm = 'l', .shift = 'L'},
                [KEY_M] = {.norm = 'm', .shift = 'M'},
                [KEY_N] = {.norm = 'n', .shift = 'N'},
                [KEY_O] = {.norm = 'o', .shift = 'O'},
                [KEY_P] = {.norm = 'p', .shift = 'P'},
                [KEY_Q] = {.norm = 'q', .shift = 'Q'},
                [KEY_R] = {.norm = 'r', .shift = 'R'},
                [KEY_S] = {.norm = 's', .shift = 'S'},
                [KEY_T] = {.norm = 't', .shift = 'T'},
                [KEY_U] = {.norm = 'u', .shift = 'U'},
                [KEY_V] = {.norm = 'v', .shift = 'V'},
                [KEY_W] = {.norm = 'w', .shift = 'W'},
                [KEY_X] = {.norm = 'x', .shift = 'X'},
                [KEY_Y] = {.norm = 'y', .shift = 'Y'},
                [KEY_Z] = {.norm = 'z', .shift = 'Z'},
                [KEY_1] = {.norm = '1', .shift = '!'},
                [KEY_2] = {.norm = '2', .shift = '"'},
                [KEY_3] = {.norm = '3', .shift = '#'},
                [KEY_4] = {.norm = '4', .shift = '$'},
                [KEY_5] = {.norm = '5', .shift = '%'},
                [KEY_6] = {.norm = '6', .shift = '&'},
                [KEY_7] = {.norm = '7', .shift = '/'},
                [KEY_8] = {.norm = '8', .shift = '('},
                [KEY_9] = {.norm = '9', .shift = ')'},
                [KEY_0] = {.norm = '0', .shift = '='},
                [KEY_MINUS] = {.norm = '-', .shift = '_'},
                [KEY_EQUAL] = {.norm = '=', .shift = '+'},
                [KEY_LEFT_BRACE] = {.norm = '[', .shift = '{'},
                [KEY_RIGHT_BRACE] = {.norm = ']', .shift = '}'},
                [KEY_BACKSLASH] = {.norm = '\\', .shift = '|'},
                [KEY_HASHTILDE] = {.norm = 0, .shift = 0},
                [KEY_SEMICOLON] = {.norm = ';', .shift = ':'},
                [KEY_APOSTROPHE] = {.norm = '\'', .shift = '"'},
                [KEY_GRAVE] = {.norm = '`', .shift = '~'},
                [KEY_COMMA] = {.norm = ',', .shift = ';'},
                [KEY_PERIOD] = {.norm = '.', .shift = ':'},
                [KEY_SLASH] = {.norm = '/', .shift = '?'},
                [KEY_SPACE] = {.norm = ' ', .shift = ' '},
                [KEY_ENTER] = {.norm = '\n', .shift = '\n'},
                [KEY_TAB] = {.norm = '\t', .shift = '\t'},
                [KEY_ESC] = {.norm = 27, .shift = 27},
                [KEY_BACKSPACE] = {.norm = '\b', .shift = '\b'},
                [KEY_CAPS_LOCK] = {.norm = 0, .shift = 0},
                [KEY_F1] = {.norm = 0, .shift = 0},
                [KEY_F2] = {.norm = 0, .shift = 0},
                [KEY_F3] = {.norm = 0, .shift = 0},
                [KEY_F4] = {.norm = 0, .shift = 0},
                [KEY_F5] = {.norm = 0, .shift = 0},
                [KEY_F6] = {.norm = 0, .shift = 0},
                [KEY_F7] = {.norm = 0, .shift = 0},
                [KEY_F8] = {.norm = 0, .shift = 0},
                [KEY_F9] = {.norm = 0, .shift = 0},
                [KEY_F10] = {.norm = 0, .shift = 0},
                [KEY_F11] = {.norm = 0, .shift = 0},
                [KEY_F12] = {.norm = 0, .shift = 0},
                [KEY_SYSRQ] = {.norm = 0, .shift = 0},
                [KEY_SCROLL_LOCK] = {.norm = 0, .shift = 0},
                [KEY_PAUSE] = {.norm = 0, .shift = 0},
                [KEY_INSERT] = {.norm = 0, .shift = 0},
                [KEY_HOME] = {.norm = 0, .shift = 0},
                [KEY_PAGE_UP] = {.norm = 0, .shift = 0},
                [KEY_DELETE] = {.norm = 0, .shift = 0},
                [KEY_END] = {.norm = 0, .shift = 0},
                [KEY_PAGE_DOWN] = {.norm = 0, .shift = 0},
                [KEY_RIGHT] = {.norm = 0, .shift = 0},
                [KEY_LEFT] = {.norm = 0, .shift = 0},
                [KEY_DOWN] = {.norm = 0, .shift = 0},
                [KEY_UP] = {.norm = 0, .shift = 0},
                [KEY_NUM_LOCK] = {.norm = 0, .shift = 0},
                [KEY_KP_SLASH] = {.norm = '/', .shift = '/'},
                [KEY_KP_ASTERISK] = {.norm = '*', .shift = '*'},
                [KEY_KP_MINUS] = {.norm = '-', .shift = '-'},
                [KEY_KP_PLUS] = {.norm = '+', .shift = '+'},
                [KEY_KP_ENTER] = {.norm = '\n', .shift = '\n'},
                [KEY_KP_1] = {.norm = '1', .shift = '1'},
                [KEY_KP_2] = {.norm = '2', .shift = '2'},
                [KEY_KP_3] = {.norm = '3', .shift = '3'},
                [KEY_KP_4] = {.norm = '4', .shift = '4'},
                [KEY_KP_5] = {.norm = '5', .shift = '5'},
                [KEY_KP_6] = {.norm = '6', .shift = '6'},
                [KEY_KP_7] = {.norm = '7', .shift = '7'},
                [KEY_KP_8] = {.norm = '8', .shift = '8'},
                [KEY_KP_9] = {.norm = '9', .shift = '9'},
                [KEY_KP_0] = {.norm = '0', .shift = '0'},
                [KEY_KP_PERIOD] = {.norm = '.', .shift = '.'},
                [KEY_102ND] = {.norm = '\\', .shift = '|'},
                [KEY_KP_EQUAL] = {.norm = '=', .shift = '='},
                [KEY_KP_COMMA] = {.norm = ',', .shift = ','},
                [KEY_KP_LEFT_PAREN] = {.norm = '(', .shift = '('},
                [KEY_KP_RIGHT_PAREN] = {.norm = ')', .shift = ')'},
                [KEY_LEFT_CTRL] = {.norm = 0, .shift = 0},
                [KEY_LEFT_SHIFT] = {.norm = 0, .shift = 0},
                [KEY_LEFT_ALT] = {.norm = 0, .shift = 0},
                [KEY_LEFT_SUPER] = {.norm = 0, .shift = 0},
                [KEY_RIGHT_CTRL] = {.norm = 0, .shift = 0},
                [KEY_RIGHT_SHIFT] = {.norm = 0, .shift = 0},
                [KEY_RIGHT_ALT] = {.norm = 0, .shift = 0},
                [KEY_RIGHT_SUPER] = {.norm = 0, .shift = 0},
            },
};

char kbd_ascii(keycode_t code, kbd_mods_t mods)
{
    if (code < 0 || code >= UINT8_MAX)
    {
        return '\0';
    }
    else
    {
        return mods & KBD_MOD_SHIFT ? keymap.map[code].shift : keymap.map[code].norm;
    }
}
