#include "ps2.h"

#include <sys/kbd.h>

#include "tty.h"
#include "io.h"
#include "heap.h"
#include "irq.h"
#include "time.h"
#include "debug.h"
#include "utils.h"
#include "ring.h"

static kbd_event_t eventBuffer[PS2_KEY_BUFFER_LENGTH];
static uint64_t writeIndex = 0;

static uint8_t scanCodeTable[] =
{
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
    KEY_KEYPAD_EQUAL
};

static uint8_t ps2_read(void)
{    
    uint64_t time = time_nanoseconds();

    while (time + NANOSECONDS_PER_SECOND > time_nanoseconds())
    {
        uint8_t status = io_inb(PS2_PORT_STATUS);
		if (status & PS2_STATUS_OUT_FULL)
        {
            io_wait();
            return io_inb(PS2_PORT_DATA);
        }
    }

    debug_panic("PS2 Timeout");
}

static void ps2_wait(void)
{
    uint64_t time = time_nanoseconds();

    while (time + NANOSECONDS_PER_SECOND > time_nanoseconds())
    {
        uint8_t status = io_inb(PS2_PORT_STATUS);
		if (status & PS2_STATUS_OUT_FULL)
        {
            ps2_read(); //Discard
        }
        if (!(status & (PS2_STATUS_IN_FULL | PS2_STATUS_OUT_FULL)))
        {
            return;
        }
    }

    debug_panic("PS2 Timeout");
}

static void ps2_write(uint8_t data)
{
    ps2_wait();
    io_outb(PS2_PORT_DATA, data);
}

static void ps2_cmd(uint8_t command)
{
    ps2_wait();
    io_outb(PS2_PORT_CMD, command);
}

static uint64_t ps2_kbd_scan(void)
{
    uint8_t status = io_inb(PS2_PORT_STATUS);
    if (!(status & PS2_STATUS_OUT_FULL))
    {
        return ERR;
    }
    
    uint8_t scanCode = io_inb(PS2_PORT_DATA);
    return scanCode;
}

static void ps2_kbd_irq(uint8_t irq)
{    
    uint64_t scanCode = ps2_kbd_scan();
    if (scanCode == ERR)
    {
        return;
    }

    bool released = scanCode & SCANCODE_RELEASED;
    uint64_t index = scanCode & ~SCANCODE_RELEASED;

    if (index >= sizeof(scanCodeTable)/sizeof(scanCodeTable[0]))
    {
        return;
    }
    uint8_t key = scanCodeTable[index];

    uint64_t time = time_nanoseconds();
    kbd_event_t event =
    {
        .time = {.tv_sec = time / NANOSECONDS_PER_SECOND, .tv_nsec = time % NANOSECONDS_PER_SECOND},
        .type = released ? KBD_EVENT_TYPE_RELEASE : KBD_EVENT_TYPE_PRESS,
        .code = key
    };

    eventBuffer[writeIndex] = event;
    writeIndex = (writeIndex + 1) % PS2_KEY_BUFFER_LENGTH;
}

static uint64_t ps2_kbd_read(File* file, void* buffer, uint64_t count)
{
    count = ROUND_DOWN(count, sizeof(kbd_event_t));

    for (uint64_t i = 0; i < count / sizeof(kbd_event_t); i++)
    {
        if (file->position == writeIndex)
        {
            return i;
        }

        ((kbd_event_t*)buffer)[i] = eventBuffer[file->position];
        file->position = (file->position + 1) % PS2_KEY_BUFFER_LENGTH;
    }

    return count;
}

static bool ps2_kbd_read_avail(File* file)
{
    return file->position != writeIndex;
}

static void ps2_controller_init(void)
{
    ps2_cmd(PS2_CMD_KBD_DISABLE);
    ps2_cmd(PS2_CMD_AUX_DISABLE);

    io_inb(PS2_PORT_DATA); //Discard

    ps2_cmd(PS2_CMD_CFG_READ);
    uint8_t cfg = ps2_read();

    ps2_cmd(PS2_CMD_CONTROLLER_TEST);
    if (ps2_read() != 0x55)
    {
        tty_print("Controller test failed");
        tty_end_message(TTY_MESSAGE_ER);
    }

    cfg |= PS2_CFG_KBD_IRQ;

    ps2_cmd(PS2_CMD_CFG_WRITE);
    ps2_write(cfg);

    ps2_cmd(PS2_CMD_KDB_ENABLE);
    ps2_cmd(PS2_CMD_AUX_ENABLE);
}

void ps2_init(void)
{
    tty_start_message("PS2 initializing");

    ps2_controller_init();

    irq_install(ps2_kbd_irq, IRQ_KEYBOARD);

    Resource* keyboard = kmalloc(sizeof(Resource));
    resource_init(keyboard, "ps2");
    keyboard->methods.read = ps2_kbd_read;
    keyboard->methods.read_avail = ps2_kbd_read_avail;

    sysfs_expose(keyboard, "/kbd");

    tty_end_message(TTY_MESSAGE_OK);
}