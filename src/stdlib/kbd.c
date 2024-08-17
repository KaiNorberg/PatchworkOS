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

static keymap_t keymap = {.map = {
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
                          }};

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
