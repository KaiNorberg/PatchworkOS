#include "kbd.h"

#include "event_stream.h"
#include "io.h"
#include "irq.h"
#include "log.h"
#include "ps2.h"
#include "ps2/scanmap.h"
#include "sched.h"
#include "time.h"
#include "vfs.h"

#include <sys/kbd.h>
#include <sys/math.h>

static event_stream_t kbd;

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

    kbd_event_t event = {
        .time = time_uptime(),
        .type = scanCode & SCANCODE_RELEASED ? KBD_RELEASE : KBD_PRESS,
        .code = ps2_scancode_to_keycode(scanCode & ~SCANCODE_RELEASED),
    };
    event_stream_push(&kbd, &event, sizeof(kbd_event_t));
}

void ps2_kbd_init(void)
{
    ps2_cmd(PS2_CMD_KBD_TEST);
    LOG_ASSERT(ps2_read() == 0x0, "ps2 kbd test fail");

    ps2_write(PS2_SET_DEFAULTS);
    LOG_ASSERT(ps2_read() == PS2_ACK, "set defaults fail, ps2 kbd might not exist");

    ps2_write(PS2_ENABLE_DATA_REPORTING);
    LOG_ASSERT(ps2_read() == PS2_ACK, "data reporting fail");

    event_stream_init(&kbd, "/kbd", "ps2", sizeof(kbd_event_t), PS2_BUFFER_LENGTH);
    irq_install(ps2_kbd_irq, IRQ_PS2_KBD);
}
