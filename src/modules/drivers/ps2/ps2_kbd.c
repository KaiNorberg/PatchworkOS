#include "ps2_kbd.h"
#include "ps2_scanmap.h"

#include <kernel/cpu/irq.h>
#include <kernel/drivers/abstractions/kbd.h>
#include <kernel/log/log.h>

#include <stdlib.h>

static kbd_t* kbd;

static void ps2_kbd_irq(irq_t irq, void* data)
{
    (void)irq; // Unused

    ps2_kbd_irq_context_t* context = data;

    ps2_device_response_t response;
    if (PS2_READ(&response) == ERR)
    {
        LOG_WARN("failed to read PS/2 keyboard response\n");
        return;
    }

    switch (response)
    {
    case PS2_DEV_RESPONSE_ACK:
    case PS2_DEV_RESPONSE_RESEND:
    case PS2_DEV_RESPONSE_BAT_OK:
        LOG_ERR("unexpected PS/2 keyboard response: %d\n", response);
        return;
    case PS2_DEV_RESPONSE_KBD_EXTENDED:
        context->isExtended = true;
        return;
    case PS2_DEV_RESPONSE_KBD_RELEASE:
        context->isRelease = true;
        return;
    default:
        break;
    }

    ps2_scancode_t scancode = (ps2_scancode_t)response;
    kbd_event_type_t type = context->isRelease ? KBD_RELEASE : KBD_PRESS;

    keycode_t code = ps2_scancode_to_keycode(scancode, context->isExtended);
    kbd_push(kbd, type, code);

    context->isExtended = false;
    context->isRelease = false;
}

uint64_t ps2_kbd_init(ps2_device_info_t* info)
{
    if (PS2_DEV_SUB_CMD(info->device, PS2_DEV_CMD_SET_SCANCODE_SET, PS2_SCAN_CODE_SET) == ERR)
    {
        LOG_ERR("failed to set PS/2 keyboard scan code set\n");
        return ERR;
    }

    kbd = kbd_new(info->name);
    if (kbd == NULL)
    {
        LOG_ERR("failed to create PS/2 keyboard\n");
        return ERR;
    }

    ps2_kbd_irq_context_t* context = malloc(sizeof(ps2_kbd_irq_context_t));
    if (context == NULL)
    {
        kbd_free(kbd);
        LOG_ERR("failed to allocate memory for PS/2 keyboard IRQ context\n");
        return ERR;
    }
    context->isExtended = false;
    context->isRelease = false;

    irq_install(info->device == PS2_DEV_FIRST ? IRQ_PS2_FIRST_DEVICE : IRQ_PS2_SECOND_DEVICE, ps2_kbd_irq, context);
    return 0;
}
