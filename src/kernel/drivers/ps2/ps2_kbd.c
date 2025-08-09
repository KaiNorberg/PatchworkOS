#include "ps2_kbd.h"
#include "ps2_scanmap.h"

#include "cpu/port.h"
#include "cpu/irq.h"
#include "drivers/helpers/kbd.h"
#include "log/log.h"

static kbd_t* kbd;

static uint64_t ps2_kbd_scan(ps2_scancode_t* scancode)
{
    uint8_t byte;
    if (PS2_READ(&byte) == ERR)
    {
        return ERR;
    }
    ps2_scancode_from_byte(scancode, byte);
    return 0;
}

static void ps2_kbd_irq(irq_t irq)
{
    static bool isExtended = false;

    ps2_scancode_t scancode;
    uint64_t result = ps2_kbd_scan(&scancode);
    if (result == ERR)
    {
        LOG_WARN("failed to scan PS/2 keyboard\n");
        return;
    }

    if (scancode.isExtendCode)
    {
        isExtended = true;
    }
    else
    {
        kbd_event_type_t type = scancode.isReleased ? KBD_RELEASE : KBD_PRESS;

        keycode_t code = ps2_scancode_to_keycode(&scancode, isExtended);
        kbd_push(kbd, type, code);
        isExtended = false;
    }
}

uint64_t ps2_kbd_init(ps2_device_info_t* info)
{
    ps2_device_ack_t ack;
    if (ps2_device_cmd(info->device, PS2_DEV_SET_SCANCODE_SET) == ERR || ps2_device_cmd(info->device, PS2_SCAN_CODE_SET) == ERR || PS2_READ(&ack) == ERR || ack != PS2_DEVICE_ACK)
    {
        LOG_ERR("failed to set PS/2 keyboard scan code set\n");
        return ERR;
    }

    irq_install(info->device == PS2_DEVICE_FIRST ? IRQ_PS2_FIRST_DEVICE : IRQ_PS2_SECOND_DEVICE, ps2_kbd_irq);

    kbd = kbd_new("ps2");
    if (kbd == NULL)
    {
        LOG_ERR("failed to create PS/2 keyboard\n");
        return ERR;
    }

    return 0;
}
