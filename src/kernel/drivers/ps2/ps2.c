#include "ps2.h"

#include "kbd.h"
#include "mouse.h"

#include "cpu/port.h"
#include "drivers/systime/systime.h"
#include "log/log.h"

#include <assert.h>

void ps2_init(void)
{
    ps2_cmd(PS2_CMD_KBD_DISABLE);
    ps2_cmd(PS2_CMD_AUX_DISABLE);

    port_inb(PS2_PORT_DATA); // Discard

    ps2_cmd(PS2_CMD_CFG_READ);
    uint8_t cfg = ps2_read();

    ps2_cmd(PS2_CMD_CONTROLLER_TEST);
    assert(ps2_read() == 0x55 && "self test fail");

    cfg = cfg | PS2_CFG_KBD_IRQ | PS2_CFG_AUX_IRQ;

    ps2_cmd(PS2_CMD_CFG_WRITE);
    ps2_write(cfg);

    ps2_cmd(PS2_CMD_KBD_ENABLE);
    ps2_cmd(PS2_CMD_AUX_ENABLE);

    ps2_kbd_init();
    ps2_mouse_init();
}

uint8_t ps2_read(void)
{
    uint64_t time = systime_uptime();

    while (time + CLOCKS_PER_SEC > systime_uptime())
    {
        uint8_t status = port_inb(PS2_PORT_STATUS);
        if (status & PS2_STATUS_OUT_FULL)
        {
            port_wait();
            return port_inb(PS2_PORT_DATA);
        }
    }

    log_panic(NULL, "PS2 Timeout");
}

void ps2_write(uint8_t data)
{
    ps2_wait();
    port_outb(PS2_PORT_DATA, data);
}

void ps2_wait(void)
{
    uint64_t time = systime_uptime();

    while (time + CLOCKS_PER_SEC > systime_uptime())
    {
        uint8_t status = port_inb(PS2_PORT_STATUS);
        if (status & PS2_STATUS_OUT_FULL)
        {
            ps2_read(); // Discard
        }
        if (!(status & (PS2_STATUS_IN_FULL | PS2_STATUS_OUT_FULL)))
        {
            return;
        }
    }

    log_panic(NULL, "PS2 Timeout");
}

void ps2_cmd(uint8_t command)
{
    ps2_wait();
    port_outb(PS2_PORT_CMD, command);
}
