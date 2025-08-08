#include "ps2_mouse.h"

#include "cpu/port.h"
#include "drivers/helpers/mouse.h"
#include "log/log.h"
#include "cpu/irq.h"

static mouse_t* mouse;

static void ps2_mouse_handle_packet(const ps2_mouse_packet_t* packet)
{
    mouse_buttons_t buttons = (packet->flags & PS2_PACKET_BUTTON_RIGHT ? MOUSE_RIGHT : 0) |
        (packet->flags & PS2_PACKET_BUTTON_MIDDLE ? MOUSE_MIDDLE : 0) |
        (packet->flags & PS2_PACKET_BUTTON_LEFT ? MOUSE_LEFT : 0);

    mouse_push(mouse, buttons, packet->deltaX, -packet->deltaY);
}

static void ps2_mouse_irq(irq_t irq)
{
    static uint8_t index = 0;
    static ps2_mouse_packet_t packet;

    uint8_t byte;
    if (PS2_READ(&byte) == ERR)
    {
        LOG_WARN("failed to scan PS/2 mouse\n");
        return;
    }

    switch (index)
    {
    case 0:
    {
        packet.flags = byte;

        if (!(packet.flags & PS2_PACKET_ALWAYS_ONE))
        {
            LOG_WARN("mouse packet out of sync flags=0x%02X\n", packet.flags);
            index = 0;
            return;
        }

        if (packet.flags & PS2_PACKET_X_OVERFLOW)
        {
            LOG_WARN("mouse packet x overflow flags=0x%02X\n", packet.flags);
        }

        if (packet.flags & PS2_PACKET_Y_OVERFLOW)
        {
            LOG_WARN("mouse packet y overflow flags=0x%02X\n", packet.flags);
        }

        index++;
    }
    break;
    case 1:
    {
        packet.deltaX = (int8_t)byte;
        index++;
    }
    break;
    case 2:
    {
        packet.deltaY = (int8_t)byte;
        index = 0;

        if (packet.flags & PS2_PACKET_X_SIGN)
        {
            packet.deltaX |= 0xFF00;
        }

        if (packet.flags & PS2_PACKET_Y_SIGN)
        {
            packet.deltaY |= 0xFF00;
        }

        ps2_mouse_handle_packet(&packet);
    }
    break;
    }
}

uint64_t ps2_mouse_init(ps2_device_info_t* info)
{
    mouse = mouse_new("ps2");
    if (mouse == NULL)
    {
        LOG_ERR("failed to create PS/2 mouse\n");
        return ERR;
    }

    irq_install(info->device == PS2_DEVICE_FIRST ? IRQ_PS2_FIRST_DEVICE : IRQ_PS2_SECOND_DEVICE, ps2_mouse_irq);
    return 0;
}
