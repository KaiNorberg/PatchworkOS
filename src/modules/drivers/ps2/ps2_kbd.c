#include "ps2_kbd.h"
#include "ps2_scanmap.h"

#include <kernel/cpu/irq.h>
#include <kernel/log/log.h>

#include <stdlib.h>

static void ps2_kbd_irq(irq_func_data_t* data)
{
    ps2_kbd_data_t* private = data->private;

    uint64_t response = ps2_read();
    if (response == ERR)
    {
        LOG_WARN("failed to scan PS/2 keyboard\n");
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
        private
        ->isExtended = true;
        return;
    case PS2_DEV_RESPONSE_KBD_RELEASE:
        private
        ->isRelease = true;
        return;
    default:
        break;
    }

    ps2_scancode_t scancode = (ps2_scancode_t)response;
    kbd_event_type_t type = private->isRelease ? KBD_RELEASE : KBD_PRESS;

    keycode_t code = ps2_scancode_to_keycode(scancode, private->isExtended);
    kbd_push(private->kbd, type, code);

    private->isExtended = false;
    private->isRelease = false;
}

uint64_t ps2_kbd_init(ps2_device_info_t* info)
{
    if (ps2_device_sub_cmd(info->device, PS2_DEV_CMD_SET_SCANCODE_SET, PS2_SCAN_CODE_SET) == ERR)
    {
        LOG_ERR("failed to set PS/2 keyboard scan code set\n");
        return ERR;
    }

    ps2_kbd_data_t* private = malloc(sizeof(ps2_kbd_data_t));
    if (private == NULL)
    {
        LOG_ERR("failed to allocate memory for PS/2 keyboard data\n");
        return ERR;
    }

    private->kbd = kbd_new(info->name);
    if (private->kbd == NULL)
    {
        free(private);
        LOG_ERR("failed to create PS/2 keyboard\n");
        return ERR;
    }

    private->isExtended = false;
    private->isRelease = false;

    if (irq_handler_register(info->irq, ps2_kbd_irq, private) == ERR)
    {
        kbd_free(private->kbd);
        free(private);
        LOG_ERR("failed to register PS/2 keyboard IRQ handler\n");
        return ERR;
    }

    info->private = private;
    return 0;
}

void ps2_kbd_deinit(ps2_device_info_t* info)
{
    if (info->private == NULL)
    {
        return;
    }

    ps2_kbd_data_t* private = info->private;

    irq_handler_unregister(ps2_kbd_irq, info->irq);
    kbd_free(private->kbd);
    free(private);

    info->private = NULL;
}
