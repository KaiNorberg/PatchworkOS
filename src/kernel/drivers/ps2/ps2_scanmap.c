#include <kernel/drivers/ps2/ps2_scanmap.h>
#include <sys/kbd.h>

static const keycode_t set2Map[256] = {
    [0x00] = KBD_NONE,
    [0x01] = KBD_F9,
    [0x03] = KBD_F5,
    [0x04] = KBD_F3,
    [0x05] = KBD_F1,
    [0x06] = KBD_F2,
    [0x07] = KBD_F12,
    [0x09] = KBD_F10,
    [0x0A] = KBD_F8,
    [0x0B] = KBD_F6,
    [0x0C] = KBD_F4,
    [0x0D] = KBD_TAB,
    [0x0E] = KBD_GRAVE,
    [0x11] = KBD_LEFT_ALT,
    [0x12] = KBD_LEFT_SHIFT,
    [0x14] = KBD_LEFT_CTRL,
    [0x15] = KBD_Q,
    [0x16] = KBD_1,
    [0x1A] = KBD_Z,
    [0x1B] = KBD_S,
    [0x1C] = KBD_A,
    [0x1D] = KBD_W,
    [0x1E] = KBD_2,
    [0x21] = KBD_C,
    [0x22] = KBD_X,
    [0x23] = KBD_D,
    [0x24] = KBD_E,
    [0x25] = KBD_4,
    [0x26] = KBD_3,
    [0x29] = KBD_SPACE,
    [0x2A] = KBD_V,
    [0x2B] = KBD_F,
    [0x2C] = KBD_T,
    [0x2D] = KBD_R,
    [0x2E] = KBD_5,
    [0x31] = KBD_N,
    [0x32] = KBD_B,
    [0x33] = KBD_H,
    [0x34] = KBD_G,
    [0x35] = KBD_Y,
    [0x36] = KBD_6,
    [0x3A] = KBD_M,
    [0x3B] = KBD_J,
    [0x3C] = KBD_U,
    [0x3D] = KBD_7,
    [0x3E] = KBD_8,
    [0x41] = KBD_COMMA,
    [0x42] = KBD_K,
    [0x43] = KBD_I,
    [0x44] = KBD_O,
    [0x45] = KBD_0,
    [0x46] = KBD_9,
    [0x49] = KBD_PERIOD,
    [0x4A] = KBD_SLASH,
    [0x4B] = KBD_L,
    [0x4C] = KBD_SEMICOLON,
    [0x4D] = KBD_P,
    [0x4E] = KBD_MINUS,
    [0x52] = KBD_APOSTROPHE,
    [0x54] = KBD_LEFT_BRACE,
    [0x55] = KBD_EQUAL,
    [0x58] = KBD_CAPS_LOCK,
    [0x59] = KBD_RIGHT_SHIFT,
    [0x5A] = KBD_ENTER,
    [0x5B] = KBD_RIGHT_BRACE,
    [0x5D] = KBD_BACKSLASH,
    [0x66] = KBD_BACKSPACE,
    [0x69] = KBD_KP_1,
    [0x6B] = KBD_KP_4,
    [0x6C] = KBD_KP_7,
    [0x70] = KBD_KP_0,
    [0x71] = KBD_KP_PERIOD,
    [0x72] = KBD_KP_2,
    [0x73] = KBD_KP_5,
    [0x74] = KBD_KP_6,
    [0x75] = KBD_KP_8,
    [0x76] = KBD_ESC,
    [0x77] = KBD_NUM_LOCK,
    [0x78] = KBD_F11,
    [0x79] = KBD_KP_PLUS,
    [0x7A] = KBD_KP_3,
    [0x7B] = KBD_KP_MINUS,
    [0x7C] = KBD_KP_ASTERISK,
    [0x7D] = KBD_KP_9,
    [0x7E] = KBD_SCROLL_LOCK,
    [0x83] = KBD_F7,
};

static const keycode_t set2ExtendedMap[256] = {
    [0x11] = KBD_RIGHT_ALT,
    [0x14] = KBD_RIGHT_CTRL,
    [0x1F] = KBD_LEFT_SUPER,
    [0x27] = KBD_RIGHT_SUPER,
    [0x37] = KBD_SYSRQ,
    [0x4A] = KBD_KP_SLASH,
    [0x5A] = KBD_KP_ENTER,
    [0x69] = KBD_END,
    [0x6B] = KBD_LEFT,
    [0x6C] = KBD_HOME,
    [0x70] = KBD_INSERT,
    [0x71] = KBD_DELETE,
    [0x72] = KBD_DOWN,
    [0x74] = KBD_RIGHT,
    [0x75] = KBD_UP,
    [0x7A] = KBD_PAGE_DOWN,
    [0x7D] = KBD_PAGE_UP,
    [0x20] = KBD_MUTE,
    [0x22] = KBD_MEDIA_PLAY_PAUSE,
    [0x24] = KBD_MEDIA_STOP,
    [0x2E] = KBD_VOLUME_DOWN,
    [0x30] = KBD_VOLUME_UP,
};

keycode_t ps2_scancode_to_keycode(ps2_scancode_t scancode, bool isExtended)
{
    if (isExtended)
    {
        if (set2ExtendedMap[scancode] != 0)
        {
            return set2ExtendedMap[scancode];
        }
    }
    else
    {
        if (set2Map[scancode] != 0)
        {
            return set2Map[scancode];
        }
    }
    return KBD_NONE;
}
