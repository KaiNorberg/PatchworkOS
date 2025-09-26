#include "ps2_mouse.h"

#include "cpu/irq.h"
#include "drivers/helpers/mouse.h"
#include "log/log.h"
#include "mem/heap.h"

static mouse_t* mouse;

static void ps2_mouse_handle_packet(const ps2_mouse_packet_t* packet)
{
    mouse_buttons_t buttons = (packet->flags & PS2_PACKET_BUTTON_RIGHT ? MOUSE_RIGHT : 0) |
        (packet->flags & PS2_PACKET_BUTTON_MIDDLE ? MOUSE_MIDDLE : 0) |
        (packet->flags & PS2_PACKET_BUTTON_LEFT ? MOUSE_LEFT : 0);

    mouse_push(mouse, buttons, packet->deltaX, -packet->deltaY);
}

static void ps2_mouse_irq(irq_t irq, void* data)
{
    (void)irq; // Unused

    ps2_mouse_irq_context_t* context = data;

    uint8_t byte;
    if (PS2_READ(&byte) == ERR)
    {
        LOG_WARN("failed to scan PS/2 mouse\n");
        return;
    }

    switch (context->index)
    {
    case PS2_PACKET_FLAGS:
    {
        context->packet.flags = byte;

        if (!(context->packet.flags & PS2_PACKET_ALWAYS_ONE))
        {
            LOG_WARN("mouse packet out of sync flags=0x%02X\n", context->packet.flags);
            context->index = PS2_PACKET_FLAGS;
            return;
        }

        if (context->packet.flags & PS2_PACKET_X_OVERFLOW)
        {
            LOG_WARN("mouse packet x overflow flags=0x%02X\n", context->packet.flags);
        }

        if (context->packet.flags & PS2_PACKET_Y_OVERFLOW)
        {
            LOG_WARN("mouse packet y overflow flags=0x%02X\n", context->packet.flags);
        }

        context->index = PS2_PACKET_DELTA_X;
    }
    break;
    case PS2_PACKET_DELTA_X:
    {
        context->packet.deltaX = byte;
        context->index = PS2_PACKET_DELTA_Y;
    }
    break;
    case PS2_PACKET_DELTA_Y:
    {
        context->packet.deltaY = (int8_t)byte;
        context->index = PS2_PACKET_FLAGS;

        if (context->packet.flags & PS2_PACKET_X_SIGN)
        {
            context->packet.deltaX |= 0xFF00;
        }

        if (context->packet.flags & PS2_PACKET_Y_SIGN)
        {
            context->packet.deltaY |= 0xFF00;
        }

        ps2_mouse_handle_packet(&context->packet);
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

    ps2_mouse_irq_context_t* context = heap_alloc(sizeof(ps2_mouse_irq_context_t), HEAP_NONE);
    if (context == NULL)
    {
        mouse_free(mouse);
        LOG_ERR("failed to allocate memory for PS/2 mouse IRQ context\n");
        return ERR;
    }
    context->index = 0;

    if (PS2_DEV_CMD(info->device, PS2_DEV_CMD_SET_DEFAULTS) == ERR)
    {
        LOG_ERR("failed to set default PS/2 mouse settings\n");
        heap_free(context);
        mouse_free(mouse);
        return ERR;
    }

    irq_install(info->device == PS2_DEV_FIRST ? IRQ_PS2_FIRST_DEVICE : IRQ_PS2_SECOND_DEVICE, ps2_mouse_irq, context);
    return 0;
}
