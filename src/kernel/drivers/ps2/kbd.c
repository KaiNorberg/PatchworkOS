#include "kbd.h"

#include "ps2.h"
#include "scanmap.h"

#include "cpu/irq.h"
#include "cpu/port.h"
#include "drivers/kbd.h"
#include "log/log.h"
#include "log/panic.h"

#include <assert.h>
#include <sys/kbd.h>
#include <sys/math.h>

static kbd_t* kbd;

static bool isExtended;

static uint64_t ps2_kbd_scan(void)
{
    uint8_t status = port_inb(PS2_PORT_STATUS);
    if (!(status & PS2_STATUS_OUT_FULL))
    {
        return ERR;
    }

    uint8_t scanCode = port_inb(PS2_PORT_DATA);
    return scanCode;
}

static void ps2_kbd_irq(uint8_t irq)
{
    uint64_t scancode = ps2_kbd_scan();
    if (scancode == ERR)
    {
        return;
    }

    if (scancode == PS2_EXTENDED_CODE)
    {
        isExtended = true;
    }
    else
    {
        kbd_event_type_t type = scancode & SCANCODE_RELEASED ? KBD_RELEASE : KBD_PRESS;
        keycode_t code = ps2_scancode_to_keycode(isExtended, scancode & ~SCANCODE_RELEASED);
        kbd_push(kbd, type, code);
        isExtended = false;
    }
}

void ps2_kbd_init(void)
{
    isExtended = false;

    ps2_cmd(PS2_CMD_KBD_TEST);
    if (ps2_read() != 0x0)
    {
        panic(NULL, "ps2 kbd test fail");
    }

    ps2_write(PS2_SET_DEFAULTS);
    if (ps2_read() != PS2_ACK)
    {
        panic(NULL, "ps2 kbd set defaults fail, kbd might not exist");
    }

    ps2_write(PS2_ENABLE_DATA_REPORTING);
    if (ps2_read() != PS2_ACK)
    {
        panic(NULL, "ps2 kbd data reporting fail");
    }

    kbd = kbd_new("ps2");
    irq_install(ps2_kbd_irq, IRQ_PS2_KBD);
    LOG_INFO("ps2: kbd\n");
}
