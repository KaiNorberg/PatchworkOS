#include "ps2.h"

#include "tty.h"
#include "io.h"
#include "heap.h"
#include "irq.h"

static uint8_t keyBuffer[UINT8_MAX];
static uint8_t shiftCount = 0;

static const char scanCodeTable[] =
{
    0,  0, '1', '2', '3', '4', '5', '6', '7', '8',
    '9', '0', '-', '=', BACKSPACE, '\t', 'q', 'w', 'e', 'r',
    't', 'y', 'u', 'i', 'o', 'p', '[', ']', ENTER, CONTROL,
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',
    '\'', '`', LEFT_SHIFT, '\\', 'z', 'x', 'c', 'v', 'b', 'n',
    'm', ',', '.', '/', 0, '*', 0, ' ', CAPS_LOCK, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, ARROW_UP, PAGE_UP, '-', ARROW_LEFT, 0, ARROW_RIGHT, '+', 0,
    ARROW_DOWN, PAGE_DOWN, 0, 0, 0, 0, 0, 0, 0, 0
};

static const char shiftedScanCodeTable[] =
{
    0,  0, '!', '"', '#', '$', '%', '&', '/', '(',
    ')', '=', '-', '=', BACKSPACE, '\t', 'Q', 'W', 'E', 'R', 
    'T', 'Y', 'U', 'I', 'O', 'P', '[', ']', ENTER, CONTROL, 
    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ';',  
    '\'', '`', LEFT_SHIFT, '\\', 'Z', 'X', 'C', 'V', 'B', 'N', 
    'M', ',', '.', '/', 0, '*', 0, ' ', CAPS_LOCK, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, ARROW_UP, PAGE_UP, '-', ARROW_LEFT, 0, ARROW_RIGHT, '+', 0, 
    ARROW_DOWN, PAGE_DOWN, 0, 0, 0, 0, 0, 0, 0, 0
};

static uint8_t ps2_read()
{
    return io_inb(PS2_DATA_PORT);
}

static void ps2_write(uint8_t data)
{
   io_outb(PS2_DATA_PORT, data);
   io_wait();
}

static void ps2_cmd(uint8_t command)
{
    io_outb(PS2_CMD_PORT, command);
    io_wait();
}

static void ps2_controller_init()
{
    ps2_cmd(PS2_CMD_READ_CFG);
    uint8_t cfg = ps2_read();

    ps2_cmd(PS2_CMD_TEST_CONTROLLER);
    if (ps2_read() != 0x55)
    {
        tty_print("Controller test failed");
        tty_end_message(TTY_MESSAGE_ER);
    }

    //Compatiblity with some hardware.
    ps2_cmd(PS2_CMD_WRITE_CFG);
    ps2_write(cfg);
}

void ps2_keyboard_irq(uint8_t irq)
{    
    uint8_t scanCode = ps2_read();
    
    /*tty_acquire();
    tty_printx(scanCode);
    tty_print(" ");
    tty_release();*/

    bool released = scanCode & SCANCODE_RELEASED;
    scanCode &= ~SCANCODE_RELEASED;

    uint8_t key = shiftCount != 0 ? shiftedScanCodeTable[scanCode] : scanCodeTable[scanCode];
    if (key == CAPS_LOCK || key == LEFT_SHIFT)
    {
        if (released)
        {
            shiftCount--;
        }
        else
        {
            shiftCount++;
        }
    }

    if (released)
    {
        keyBuffer[key]--;
    }
    else
    {
        tty_acquire();
        tty_put(key);
        tty_release();

        keyBuffer[key]++;
        keyBuffer[0] = key;
    }
}

void ps2_init()
{
    tty_start_message("PS2 initializing");

    ps2_controller_init();

    irq_install_handler(ps2_keyboard_irq, IRQ_KEYBOARD);

    Ps2Keyboard* keyboard = kmalloc(sizeof(Ps2Keyboard));
    resource_init(&keyboard->base, "0");

    sysfs_expose(&keyboard->base, "/keyboard");

    tty_end_message(TTY_MESSAGE_OK);
}