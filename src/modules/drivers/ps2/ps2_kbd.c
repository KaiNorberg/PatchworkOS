#include "ps2_kbd.h"
#include "ps2_scanmap.h"

#include <kernel/cpu/irq.h>
#include <kernel/log/log.h>

#include <stdlib.h>

static void ps2_kbd_irq(irq_func_data_t* data)
{
    ps2_kbd_t* kbd = data->data;

    uint64_t response = ps2_read_no_wait();
    if (response == _FAIL)
    {
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
        kbd->flags |= PS2_KBD_EXTENDED;
        return;
    case PS2_DEV_RESPONSE_KBD_RELEASE:
        kbd->flags |= PS2_KBD_RELEASE;
        return;
    default:
        break;
    }

    ps2_scancode_t scancode = (ps2_scancode_t)response;
    keycode_t code = ps2_scancode_to_keycode(scancode, kbd->flags & PS2_KBD_EXTENDED);

    if (kbd->flags & PS2_KBD_RELEASE)
    {
        kbd_release(kbd->kbd, code);
    }
    else
    {
        kbd_press(kbd->kbd, code);
    }

    kbd->flags = PS2_KBD_NONE;
}

uint64_t ps2_kbd_init(ps2_device_info_t* info)
{
    if (ps2_device_sub_cmd(info->device, PS2_DEV_CMD_SET_SCANCODE_SET, PS2_SCAN_CODE_SET) == _FAIL)
    {
        LOG_ERR("failed to set PS/2 keyboard scan code set\n");
        return _FAIL;
    }

    ps2_kbd_t* kbd = malloc(sizeof(ps2_kbd_t));
    if (kbd == NULL)
    {
        LOG_ERR("failed to allocate memory for PS/2 keyboard data\n");
        return _FAIL;
    }
    kbd->flags = PS2_KBD_NONE;
    kbd->kbd = kbd_new(info->name);
    if (kbd->kbd == NULL)
    {
        free(kbd);
        LOG_ERR("failed to create PS/2 keyboard\n");
        return _FAIL;
    }

    info->data = kbd;
    return 0;
}

uint64_t ps2_kbd_irq_register(ps2_device_info_t* info)
{
    ps2_kbd_t* kbd = info->data;
    if (kbd == NULL)
    {
        LOG_ERR("PS/2 keyboard data is NULL during IRQ registration\n");
        errno = EINVAL;
        return _FAIL;
    }

    if (irq_handler_register(info->irq, ps2_kbd_irq, kbd) == _FAIL)
    {
        LOG_ERR("failed to register PS/2 keyboard IRQ handler\n");
        return _FAIL;
    }

    return 0;
}

void ps2_kbd_deinit(ps2_device_info_t* info)
{
    if (info->data == NULL)
    {
        return;
    }

    ps2_kbd_t* kbd = info->data;

    irq_handler_unregister(ps2_kbd_irq, info->irq);
    kbd_free(kbd->kbd);
    free(kbd);

    info->data = NULL;
}
