#include "keyboard.h"

#include "io.h"
#include "irq.h"
#include "log.h"
#include "ps2.h"
#include "sched.h"
#include "sysfs.h"
#include "time.h"

#include <sys/keyboard.h>
#include <sys/math.h>

static keyboard_event_t eventBuffer[PS2_BUFFER_LENGTH];
static uint64_t writeIndex = 0;

static resource_t keyboard;

static uint8_t scanCodeTable[] = {
    0,
    KEY_ESC,
    KEY_1,
    KEY_2,
    KEY_3,
    KEY_4,
    KEY_5,
    KEY_6,
    KEY_7,
    KEY_8,
    KEY_9,
    KEY_0,
    KEY_MINUS,
    KEY_EQUAL,
    KEY_BACKSPACE,
    KEY_TAB,
    KEY_Q,
    KEY_W,
    KEY_E,
    KEY_R,
    KEY_T,
    KEY_Y,
    KEY_U,
    KEY_I,
    KEY_O,
    KEY_P,
    KEY_OPEN_BRACKET,
    KEY_CLOSE_BRACKET,
    KEY_ENTER,
    KEY_LEFT_CTRL,
    KEY_A,
    KEY_S,
    KEY_D,
    KEY_F,
    KEY_G,
    KEY_H,
    KEY_J,
    KEY_K,
    KEY_L,
    KEY_SEMICOLON,
    KEY_APOSTROPHE,
    KEY_BACKTICK,
    KEY_LEFT_SHIFT,
    KEY_BACKSLASH,
    KEY_Z,
    KEY_X,
    KEY_C,
    KEY_V,
    KEY_B,
    KEY_N,
    KEY_M,
    KEY_COMMA,
    KEY_PERIOD,
    KEY_SLASH,
    KEY_RIGHT_SHIFT,
    KEY_KEYPAD_MULTIPLY,
    KEY_LEFT_ALT,
    KEY_SPACE,
    KEY_CAPS_LOCK,
    KEY_F1,
    KEY_F2,
    KEY_F3,
    KEY_F4,
    KEY_F5,
    KEY_F6,
    KEY_F7,
    KEY_F8,
    KEY_F9,
    KEY_F10,
    KEY_NUM_LOCK,
    KEY_SCROLL_LOCK,
    KEY_KEYPAD_7,
    KEY_KEYPAD_8,
    KEY_KEYPAD_9,
    KEY_KEYPAD_MINUS,
    KEY_KEYPAD_4,
    KEY_KEYPAD_5,
    KEY_KEYPAD_6,
    KEY_KEYPAD_PLUS,
    KEY_KEYPAD_1,
    KEY_KEYPAD_2,
    KEY_KEYPAD_3,
    KEY_KEYPAD_0,
    0,
    KEY_KEYPAD_PERIOD,
    KEY_SYSREQ,
    KEY_EUROPE_2,
    KEY_F11,
    KEY_F12,
    KEY_KEYPAD_EQUAL,
};

static uint64_t ps2_keyboard_scan(void)
{
    uint8_t status = io_inb(PS2_PORT_STATUS);
    if (!(status & PS2_STATUS_OUT_FULL))
    {
        return ERR;
    }

    uint8_t scanCode = io_inb(PS2_PORT_DATA);
    return scanCode;
}

static void ps2_keyboard_irq(uint8_t irq)
{
    uint64_t scanCode = ps2_keyboard_scan();
    if (scanCode == ERR)
    {
        return;
    }

    bool released = scanCode & SCANCODE_RELEASED;
    uint64_t index = scanCode & ~SCANCODE_RELEASED;

    if (index >= sizeof(scanCodeTable) / sizeof(scanCodeTable[0]))
    {
        return;
    }
    uint8_t key = scanCodeTable[index];

    keyboard_event_t event = {
        .time = time_uptime(),
        .type = released ? KEYBOARD_RELEASE : KEYBOARD_PRESS,
        .code = key,
    };

    eventBuffer[writeIndex] = event;
    writeIndex = (writeIndex + 1) % PS2_BUFFER_LENGTH;
}

static uint64_t ps2_keyboard_read(file_t* file, void* buffer, uint64_t count)
{
    SCHED_WAIT(file->position != writeIndex, NEVER);

    count = ROUND_DOWN(count, sizeof(keyboard_event_t));

    for (uint64_t i = 0; i < count / sizeof(keyboard_event_t); i++)
    {
        if (file->position == writeIndex)
        {
            return i;
        }

        ((keyboard_event_t*)buffer)[i] = eventBuffer[file->position];
        file->position = (file->position + 1) % PS2_BUFFER_LENGTH;
    }

    return count;
}

static bool ps2_keyboard_read_avail(file_t* file)
{
    return file->position != writeIndex;
}

static file_ops_t fileOps = {
    .read = ps2_keyboard_read,
    .read_avail = ps2_keyboard_read_avail,
};

void ps2_keyboard_init(void)
{
    ps2_cmd(PS2_CMD_KEYBOARD_TEST);
    LOG_ASSERT(ps2_read() == 0x0, "ps2 keyboard not found");

    irq_install(ps2_keyboard_irq, IRQ_KEYBOARD);

    sysfs_expose("/keyboard", "ps2", &fileOps);
}
