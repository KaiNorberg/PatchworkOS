#include "mouse.h"

#include "cpu/irq.h"
#include "cpu/port.h"
#include "drivers/mouse.h"
#include "drivers/systime/systime.h"
#include "fs/vfs.h"
#include "log/log.h"
#include "log/panic.h"
#include "ps2.h"
#include "sched/sched.h"
#include "stdlib.h"

#include <assert.h>
#include <sys/math.h>
#include <sys/mouse.h>

static mouse_t* mouse;

static void ps2_mouse_handle_packet(const ps2_mouse_packet_t* packet)
{
    mouse_buttons_t buttons = (packet->flags & PS2_PACKET_BUTTON_RIGHT ? MOUSE_RIGHT : 0) |
        (packet->flags & PS2_PACKET_BUTTON_MIDDLE ? MOUSE_MIDDLE : 0) |
        (packet->flags & PS2_PACKET_BUTTON_LEFT ? MOUSE_LEFT : 0);

    mouse_push(mouse, buttons, (int16_t)packet->deltaX - (((int16_t)packet->flags << 4) & 0x100),
        -((int16_t)packet->deltaY - (((int16_t)packet->flags << 3) & 0x100)));
}

static uint64_t ps2_mouse_scan(void)
{
    uint8_t status = port_inb(PS2_PORT_STATUS);
    if (!(status & PS2_STATUS_OUT_FULL))
    {
        return ERR;
    }

    uint8_t data = port_inb(PS2_PORT_DATA);
    return data;
}

static void ps2_mouse_irq(uint8_t irq)
{
    static uint64_t index = 0;
    static ps2_mouse_packet_t packet;

    uint64_t data = ps2_mouse_scan();
    if (data == ERR)
    {
        return;
    }

    switch (index)
    {
    case 0:
    {
        packet.flags = data;
        index++;
    }
    break;
    case 1:
    {
        packet.deltaX = data;
        index++;
    }
    break;
    case 2:
    {
        packet.deltaY = data;
        index = 0;

        ps2_mouse_handle_packet(&packet);
    }
    break;
    }
}

void ps2_mouse_init(void)
{
    ps2_cmd(PS2_CMD_AUX_TEST);
    if (ps2_read() != 0x0)
    {
        panic(NULL, "ps2 mouse test fail");
    }

    ps2_cmd(PS2_CMD_AUX_WRITE);
    ps2_write(PS2_SET_DEFAULTS);

    if (ps2_read() != PS2_ACK)
    {
        panic(NULL, "ps2 mouse set defaults fail, kbd might not exist");
    }

    ps2_cmd(PS2_CMD_AUX_WRITE);
    ps2_write(PS2_ENABLE_DATA_REPORTING);

    if (ps2_read() != PS2_ACK)
    {
        panic(NULL, "ps2 mouse data reporting fail");
    }

    mouse = mouse_new("ps2");
    if (mouse == NULL)
    {
        panic(NULL, "failed to create ps2 mouse");
    }

    irq_install(ps2_mouse_irq, IRQ_PS2_AUX);
    LOG_INFO("ps2 mouse initialized\n");
}
