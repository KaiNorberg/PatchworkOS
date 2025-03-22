#include "kbd.h"

#include "../kbd.h"
#include "io.h"
#include "irq.h"
#include "log.h"
#include "ps2.h"
#include "ps2/scanmap.h"

#include <sys/kbd.h>
#include <sys/math.h>

static kbd_t* kbd;

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

    kbd_event_type_t type = scanCode & SCANCODE_RELEASED ? KBD_RELEASE : KBD_PRESS;
    keycode_t code = ps2_scancode_to_keycode(scanCode & ~SCANCODE_RELEASED);
    kbd_push(kbd, type, code);
}

void ps2_kbd_init(void)
{
    ps2_cmd(PS2_CMD_KBD_TEST);
    ASSERT_PANIC(ps2_read() == 0x0, "ps2 kbd test fail");

    ps2_write(PS2_SET_DEFAULTS);
    ASSERT_PANIC(ps2_read() == PS2_ACK, "set defaults fail, ps2 kbd might not exist");

    ps2_write(PS2_ENABLE_DATA_REPORTING);
    ASSERT_PANIC(ps2_read() == PS2_ACK, "data reporting fail");

    kbd = kbd_new("ps2");
    irq_install(ps2_kbd_irq, IRQ_PS2_KBD);
}
