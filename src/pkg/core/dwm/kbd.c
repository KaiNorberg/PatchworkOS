#include "kbd.h"

/// @todo Use the configuration system for keymaps

typedef struct
{
    char norm;
    char shift;
} keymap_entry_t;

typedef struct
{
    keymap_entry_t map[UINT8_MAX];
} keymap_t;

static keymap_t keymap = {
    .map =
        {
            [KBD_A] = {.norm = 'a', .shift = 'A'},
            [KBD_B] = {.norm = 'b', .shift = 'B'},
            [KBD_C] = {.norm = 'c', .shift = 'C'},
            [KBD_D] = {.norm = 'd', .shift = 'D'},
            [KBD_E] = {.norm = 'e', .shift = 'E'},
            [KBD_F] = {.norm = 'f', .shift = 'F'},
            [KBD_G] = {.norm = 'g', .shift = 'G'},
            [KBD_H] = {.norm = 'h', .shift = 'H'},
            [KBD_I] = {.norm = 'i', .shift = 'I'},
            [KBD_J] = {.norm = 'j', .shift = 'J'},
            [KBD_K] = {.norm = 'k', .shift = 'K'},
            [KBD_L] = {.norm = 'l', .shift = 'L'},
            [KBD_M] = {.norm = 'm', .shift = 'M'},
            [KBD_N] = {.norm = 'n', .shift = 'N'},
            [KBD_O] = {.norm = 'o', .shift = 'O'},
            [KBD_P] = {.norm = 'p', .shift = 'P'},
            [KBD_Q] = {.norm = 'q', .shift = 'Q'},
            [KBD_R] = {.norm = 'r', .shift = 'R'},
            [KBD_S] = {.norm = 's', .shift = 'S'},
            [KBD_T] = {.norm = 't', .shift = 'T'},
            [KBD_U] = {.norm = 'u', .shift = 'U'},
            [KBD_V] = {.norm = 'v', .shift = 'V'},
            [KBD_W] = {.norm = 'w', .shift = 'W'},
            [KBD_X] = {.norm = 'x', .shift = 'X'},
            [KBD_Y] = {.norm = 'y', .shift = 'Y'},
            [KBD_Z] = {.norm = 'z', .shift = 'Z'},

            [KBD_1] = {.norm = '1', .shift = '!'},
            [KBD_2] = {.norm = '2', .shift = '@'},
            [KBD_3] = {.norm = '3', .shift = '#'},
            [KBD_4] = {.norm = '4', .shift = '$'},
            [KBD_5] = {.norm = '5', .shift = '%'},
            [KBD_6] = {.norm = '6', .shift = '^'},
            [KBD_7] = {.norm = '7', .shift = '&'},
            [KBD_8] = {.norm = '8', .shift = '*'},
            [KBD_9] = {.norm = '9', .shift = '('},
            [KBD_0] = {.norm = '0', .shift = ')'},

            [KBD_ENTER] = {.norm = '\n', .shift = '\n'},
            [KBD_ESC] = {.norm = 0x1B, .shift = 0x1B},
            [KBD_BACKSPACE] = {.norm = 0x08, .shift = 0x08},
            [KBD_TAB] = {.norm = '\t', .shift = '\t'},
            [KBD_SPACE] = {.norm = ' ', .shift = ' '},
            [KBD_MINUS] = {.norm = '-', .shift = '_'},
            [KBD_EQUAL] = {.norm = '=', .shift = '+'},
            [KBD_LEFT_BRACE] = {.norm = '[', .shift = '{'},
            [KBD_RIGHT_BRACE] = {.norm = ']', .shift = '}'},
            [KBD_BACKSLASH] = {.norm = '\\', .shift = '|'},
            [KBD_HASHTILDE] = {.norm = '#', .shift = '~'},
            [KBD_SEMICOLON] = {.norm = ';', .shift = ':'},
            [KBD_APOSTROPHE] = {.norm = '\'', .shift = '"'},
            [KBD_GRAVE] = {.norm = '`', .shift = '~'},
            [KBD_COMMA] = {.norm = ',', .shift = '<'},
            [KBD_PERIOD] = {.norm = '.', .shift = '>'},
            [KBD_SLASH] = {.norm = '/', .shift = '?'},

            [KBD_KP_0] = {.norm = '0', .shift = KBD_NONE},
            [KBD_KP_1] = {.norm = '1', .shift = KBD_NONE},
            [KBD_KP_2] = {.norm = '2', .shift = KBD_NONE},
            [KBD_KP_3] = {.norm = '3', .shift = KBD_NONE},
            [KBD_KP_4] = {.norm = '4', .shift = KBD_NONE},
            [KBD_KP_5] = {.norm = '5', .shift = KBD_NONE},
            [KBD_KP_6] = {.norm = '6', .shift = KBD_NONE},
            [KBD_KP_7] = {.norm = '7', .shift = KBD_NONE},
            [KBD_KP_8] = {.norm = '8', .shift = KBD_NONE},
            [KBD_KP_9] = {.norm = '9', .shift = KBD_NONE},
            [KBD_KP_PERIOD] = {.norm = '.', .shift = KBD_NONE},
            [KBD_KP_SLASH] = {.norm = '/', .shift = KBD_NONE},
            [KBD_KP_ASTERISK] = {.norm = '*', .shift = KBD_NONE},
            [KBD_KP_MINUS] = {.norm = '-', .shift = KBD_NONE},
            [KBD_KP_PLUS] = {.norm = '+', .shift = KBD_NONE},
            [KBD_KP_ENTER] = {.norm = '\n', .shift = '\n'},
            [KBD_KP_EQUAL] = {.norm = '=', .shift = '='},
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
