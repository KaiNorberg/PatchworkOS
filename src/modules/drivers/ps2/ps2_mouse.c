#include "ps2_mouse.h"

#include <kernel/cpu/irq.h>
#include <kernel/log/log.h>

#include <stdlib.h>

static void ps2_mouse_handle_packet(mouse_t* mouse, const ps2_mouse_packet_t* packet)
{
    mouse_buttons_t buttons = (packet->flags & PS2_PACKET_BUTTON_RIGHT ? MOUSE_RIGHT : 0) |
        (packet->flags & PS2_PACKET_BUTTON_MIDDLE ? MOUSE_MIDDLE : 0) |
        (packet->flags & PS2_PACKET_BUTTON_LEFT ? MOUSE_LEFT : 0);

    mouse_push(mouse, buttons, packet->deltaX, -packet->deltaY);
}

static void ps2_mouse_irq(irq_func_data_t* data)
{
    ps2_mouse_data_t* private = data->private;
    uint64_t byte = ps2_read_no_wait();
    if (byte == ERR)
    {
        return;
    }

    switch (private->index)
    {
    case PS2_PACKET_FLAGS:
    {
        private->packet.flags = byte;

        if (!(private->packet.flags & PS2_PACKET_ALWAYS_ONE))
        {
            LOG_WARN("mouse packet out of sync flags=0x%02X\n", private->packet.flags);
            private->index = PS2_PACKET_FLAGS;
            return;
        }

        if (private->packet.flags & PS2_PACKET_X_OVERFLOW)
        {
            LOG_WARN("mouse packet x overflow flags=0x%02X\n", private->packet.flags);
        }

        if (private->packet.flags & PS2_PACKET_Y_OVERFLOW)
        {
            LOG_WARN("mouse packet y overflow flags=0x%02X\n", private->packet.flags);
        }

        private->index = PS2_PACKET_DELTA_X;
    }
    break;
    case PS2_PACKET_DELTA_X:
    {
        private->packet.deltaX = (int8_t)byte;
        private->index = PS2_PACKET_DELTA_Y;
    }
    break;
    case PS2_PACKET_DELTA_Y:
    {
        private->packet.deltaY = (int8_t)byte;
        private->index = PS2_PACKET_FLAGS;
        ps2_mouse_handle_packet(private->mouse, &private->packet);
    }
    break;
    }
}

uint64_t ps2_mouse_init(ps2_device_info_t* info)
{
    ps2_mouse_data_t* private = malloc(sizeof(ps2_mouse_data_t));
    if (private == NULL)
    {
        LOG_ERR("failed to allocate memory for PS/2 mouse data\n");
        return ERR;
    }

    private->mouse = mouse_new(info->name);
    if (private->mouse == NULL)
    {
        free(private);
        LOG_ERR("failed to create PS/2 mouse\n");
        return ERR;
    }

    private->index = 0;

    if (ps2_device_cmd(info->device, PS2_DEV_CMD_SET_DEFAULTS) == ERR)
    {
        mouse_free(private->mouse);
        free(private);
        LOG_ERR("failed to set default PS/2 mouse settings\n");
        return ERR;
    }

    info->private = private;
    return 0;
}

uint64_t ps2_mouse_irq_register(ps2_device_info_t* info)
{
    ps2_mouse_data_t* private = info->private;
    if (private == NULL)
    {
        LOG_ERR("PS/2 mouse data is NULL during IRQ registration\n");
        errno = EINVAL;
        return ERR;
    }

    if (irq_handler_register(info->irq, ps2_mouse_irq, private) == ERR)
    {
        LOG_ERR("failed to register PS/2 mouse IRQ handler\n");
        return ERR;
    }

    return 0;
}

void ps2_mouse_deinit(ps2_device_info_t* info)
{
    if (info->private == NULL)
    {
        return;
    }

    ps2_mouse_data_t* private = info->private;

    irq_handler_unregister(ps2_mouse_irq, info->irq);
    mouse_free(private->mouse);
    free(private);

    info->private = NULL;
}
