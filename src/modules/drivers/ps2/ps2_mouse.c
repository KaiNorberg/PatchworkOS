#include "ps2_mouse.h"

#include <kernel/cpu/irq.h>
#include <kernel/log/log.h>

#include <stdlib.h>

static void ps2_mouse_handle_packet(mouse_t* mouse, ps2_mouse_t* ps2)
{
    if (ps2->current.deltaX != 0)
    {
        mouse_move_x(mouse, ps2->current.deltaX);
    }

    if (ps2->current.deltaY != 0)
    {
        mouse_move_y(mouse, -ps2->current.deltaY);
    }

    if ((ps2->prev.flags & PS2_PACKET_BUTTON_LEFT) != (ps2->current.flags & PS2_PACKET_BUTTON_LEFT))
    {
        if (ps2->current.flags & PS2_PACKET_BUTTON_LEFT)
        {
            mouse_press(mouse, 1);
        }
        else
        {
            mouse_release(mouse, 1);
        }
    }

    if ((ps2->prev.flags & PS2_PACKET_BUTTON_RIGHT) != (ps2->current.flags & PS2_PACKET_BUTTON_RIGHT))
    {
        if (ps2->current.flags & PS2_PACKET_BUTTON_RIGHT)
        {
            mouse_press(mouse, 2);
        }
        else
        {
            mouse_release(mouse, 2);
        }
    }

    if ((ps2->prev.flags & PS2_PACKET_BUTTON_MIDDLE) != (ps2->current.flags & PS2_PACKET_BUTTON_MIDDLE))
    {
        if (ps2->current.flags & PS2_PACKET_BUTTON_MIDDLE)
        {
            mouse_press(mouse, 3);
        }
        else
        {
            mouse_release(mouse, 3);
        }
    }


    ps2->prev = ps2->current;
}

static void ps2_mouse_irq(irq_func_data_t* data)
{
    ps2_mouse_t* mouse = data->private;
    uint64_t byte = ps2_read_no_wait();
    if (byte == ERR)
    {
        return;
    }

    switch (mouse->index)
    {
    case PS2_PACKET_FLAGS:
    {
        mouse->current.flags = byte;

        if (!(mouse->current.flags & PS2_PACKET_ALWAYS_ONE))
        {
            LOG_WARN("mouse packet out of sync flags=0x%02X\n", mouse->current.flags);
            mouse->index = PS2_PACKET_FLAGS;
            return;
        }

        if (mouse->current.flags & PS2_PACKET_X_OVERFLOW)
        {
            LOG_WARN("mouse packet x overflow flags=0x%02X\n", mouse->current.flags);
        }

        if (mouse->current.flags & PS2_PACKET_Y_OVERFLOW)
        {
            LOG_WARN("mouse packet y overflow flags=0x%02X\n", mouse->current.flags);
        }

        mouse->index = PS2_PACKET_DELTA_X;
    }
    break;
    case PS2_PACKET_DELTA_X:
    {
        mouse->current.deltaX = (int8_t)byte;
        mouse->index = PS2_PACKET_DELTA_Y;
    }
    break;
    case PS2_PACKET_DELTA_Y:
    {
        mouse->current.deltaY = (int8_t)byte;
        mouse->index = PS2_PACKET_FLAGS;
        ps2_mouse_handle_packet(mouse->mouse, mouse);
    }
    break;
    }
}

uint64_t ps2_mouse_init(ps2_device_info_t* info)
{
    ps2_mouse_t* mouse = calloc(1, sizeof(ps2_mouse_t));
    if (mouse == NULL)
    {
        LOG_ERR("failed to allocate memory for PS/2 mouse data\n");
        return ERR;
    }

    mouse->mouse = mouse_new(info->name);
    if (mouse->mouse == NULL)
    {
        free(mouse);
        LOG_ERR("failed to create PS/2 mouse\n");
        return ERR;
    }

    mouse->index = 0;

    if (ps2_device_cmd(info->device, PS2_DEV_CMD_SET_DEFAULTS) == ERR)
    {
        mouse_free(mouse->mouse);
        free(mouse);
        LOG_ERR("failed to set default PS/2 mouse settings\n");
        return ERR;
    }

    info->private = mouse;
    return 0;
}

uint64_t ps2_mouse_irq_register(ps2_device_info_t* info)
{
    ps2_mouse_t* mouse = info->private;
    if (mouse == NULL)
    {
        LOG_ERR("PS/2 mouse data is NULL during IRQ registration\n");
        errno = EINVAL;
        return ERR;
    }

    if (irq_handler_register(info->irq, ps2_mouse_irq, mouse) == ERR)
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

    ps2_mouse_t* mouse = info->private;

    irq_handler_unregister(ps2_mouse_irq, info->irq);
    mouse_free(mouse->mouse);
    free(mouse);

    info->private = NULL;
}
