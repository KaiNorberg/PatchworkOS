#include "mouse.h"

#include "event_stream.h"
#include "io.h"
#include "irq.h"
#include "log.h"
#include "ps2.h"
#include "sched.h"
#include "stdlib.h"
#include "time.h"
#include "vfs.h"

#include <sys/math.h>
#include <sys/mouse.h>

static event_stream_t mouse;

static void ps2_mouse_handle_packet(const ps2_mouse_packet_t* packet)
{
    mouse_event_t event = {
        .time = time_uptime(),
        .buttons = ((packet->flags & PS2_PACKET_BUTTON_RIGHT) != 0 ? MOUSE_RIGHT : 0) |
            ((packet->flags & PS2_PACKET_BUTTON_MIDDLE) != 0 ? MOUSE_MIDDLE : 0) |
            ((packet->flags & PS2_PACKET_BUTTON_LEFT) != 0 ? MOUSE_LEFT : 0),
        .deltaX = (int16_t)packet->deltaX - (((int16_t)packet->flags << 4) & 0x100),
        .deltaY = -((int16_t)packet->deltaY - (((int16_t)packet->flags << 3) & 0x100)),
    };

    event_stream_push(&mouse, &event, sizeof(mouse_event_t));
}

static uint64_t ps2_mouse_scan(void)
{
    uint8_t status = io_inb(PS2_PORT_STATUS);
    if (!(status & PS2_STATUS_OUT_FULL))
    {
        return ERR;
    }

    uint8_t data = io_inb(PS2_PORT_DATA);
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
    LOG_ASSERT(ps2_read() == 0x0, "ps2 mouse test fail");

    ps2_cmd(PS2_CMD_AUX_WRITE);
    ps2_write(PS2_SET_DEFAULTS);

    LOG_ASSERT(ps2_read() == PS2_ACK, "set defaults fail, ps2 mouse might not exist");

    ps2_cmd(PS2_CMD_AUX_WRITE);
    ps2_write(PS2_ENABLE_DATA_REPORTING);

    LOG_ASSERT(ps2_read() == PS2_ACK, "data reporting fail");

    event_stream_init(&mouse, "/mouse", "ps2", sizeof(mouse_event_t), PS2_BUFFER_LENGTH);
    irq_install(ps2_mouse_irq, IRQ_PS2_AUX);
}
