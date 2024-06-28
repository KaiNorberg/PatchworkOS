#include "mouse.h"

#include "io.h"
#include "irq.h"
#include "log.h"
#include "ps2.h"
#include "sched.h"
#include "stdlib.h"
#include "sysfs.h"
#include "time.h"
#include "utils.h"

#include <sys/mouse.h>

static mouse_event_t eventBuffer[PS2_BUFFER_LENGTH];
static uint64_t writeIndex = 0;

static resource_t mouse;

static void ps2_mouse_handle_packet(const ps2_mouse_packet_t* packet)
{
    mouse_event_t event = {.time = time_uptime(),
        .buttons = ((packet->flags & PS2_PACKET_BUTTON_RIGHT) != 0 ? MOUSE_RIGHT : 0) |
            ((packet->flags & PS2_PACKET_BUTTON_MIDDLE) != 0 ? MOUSE_MIDDLE : 0) |
            ((packet->flags & PS2_PACKET_BUTTON_LEFT) != 0 ? MOUSE_LEFT : 0),
        .deltaX = (int16_t)packet->deltaX - (((int16_t)packet->flags << 4) & 0x100),
        .deltaY = -((int16_t)packet->deltaY - (((int16_t)packet->flags << 3) & 0x100))};

    eventBuffer[writeIndex] = event;
    writeIndex = (writeIndex + 1) % PS2_BUFFER_LENGTH;
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

//
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

static uint64_t ps2_mouse_read(file_t* file, void* buffer, uint64_t count)
{
    SCHED_WAIT(file->position != writeIndex, NEVER);

    count = ROUND_DOWN(count, sizeof(mouse_event_t));

    for (uint64_t i = 0; i < count / sizeof(mouse_event_t); i++)
    {
        if (file->position == writeIndex)
        {
            return i;
        }

        ((mouse_event_t*)buffer)[i] = eventBuffer[file->position];
        file->position = (file->position + 1) % PS2_BUFFER_LENGTH;
    }

    return count;
}

static bool ps2_mouse_read_avail(file_t* file)
{
    return file->position != writeIndex;
}

static void ps2_mouse_cleanup(file_t* file)
{
    resource_t* resource = file->internal;
    resource_unref(resource);
}

static uint64_t ps2_mouse_open(resource_t* resource, file_t* file)
{
    file->ops.read = ps2_mouse_read;
    file->ops.read_avail = ps2_mouse_read_avail;
    file->cleanup = ps2_mouse_cleanup;
    file->internal = resource_ref(resource);
    return 0;
}

void ps2_mouse_init(void)
{
    ps2_cmd(PS2_CMD_AUX_TEST);
    LOG_ASSERT(ps2_read() == 0x0, "ps2 mouse not found");

    ps2_cmd(PS2_CMD_WRITE_MOUSE);
    ps2_write(PS2_SET_DEFAULTS);

    LOG_ASSERT(ps2_read() == 0xFA, "set defaults fail");

    ps2_cmd(PS2_CMD_WRITE_MOUSE);
    ps2_write(PS2_ENABLE_DATA_REPORTING);

    LOG_ASSERT(ps2_read() == 0xFA, "data reporting fail");

    irq_install(ps2_mouse_irq, IRQ_MOUSE);

    resource_init(&mouse, "ps2", ps2_mouse_open, NULL);
    sysfs_expose(&mouse, "/mouse");
}
