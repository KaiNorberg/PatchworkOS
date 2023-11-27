#include "interrupts.h"

#include "kernel/io/io.h"

#include "kernel/tty/tty.h"
#include "kernel/utils/utils.h"

#include "kernel/syscall/syscall.h"

#define ENTER 0x1C
#define BACKSPACE 0x0E
#define CONTROL 0x1D
#define LEFT_SHIFT 0x2A
#define ARROW_UP 0x48
#define ARROW_DOWN 0x50
#define ARROW_LEFT 0x4B
#define ARROW_RIGHT 0x4D
#define PAGE_UP 0x49
#define PAGE_DOWN 0x51
#define CAPS_LOCK 0x3A

const char SCAN_CODE_TABLE[] =
{
    0,  0, '1', '2', '3', '4', '5', '6', '7', '8',	/* 9 */
    '9', '0', '-', '=', BACKSPACE,	/* Backspace */
    '\t',			/* Tab */
    'q', 'w', 'e', 'r',	/* 19 */
    't', 'y', 'u', 'i', 'o', 'p', '[', ']', ENTER,	/* Enter key */
    CONTROL,			/* 29   - Control */
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',	/* 39 */
    '\'', '`', LEFT_SHIFT,		/* Left shift */
    '\\', 'z', 'x', 'c', 'v', 'b', 'n',			/* 49 */
    'm', ',', '.', '/',   0,				/* Right shift */
    '*',
    0,	/* Alt */
    ' ',	/* Space bar */
    CAPS_LOCK,	/* Caps lock */
    0,	/* 59 - F1 key ... > */
    0,   0,   0,   0,   0,   0,   0,   0,
    0,	/* < ... F10 */
    0,	/* 69 - Num lock*/
    0,	/* Scroll Lock */
    0,	/* Home key */
    ARROW_UP,	/* Up Arrow */
    PAGE_UP,	/* Page Up */
    '-',
    ARROW_LEFT,	/* Left Arrow */
    0,
    ARROW_RIGHT,	/* Right Arrow */
    '+',
    0,	/* 79 - End key*/
    ARROW_DOWN,	/* Down Arrow */
    PAGE_DOWN,	/* Page Down */
    0,	/* Insert Key */
    0,	/* Delete Key */
    0,   0,   0,
    0,	/* F11 Key */
    0,	/* F12 Key */
    0,	/* All other keys are undefined */
};

const char SHIFTED_SCAN_CODE_TABLE[] =
{
    0,  0, '!', '"', '#', '$', '%', '&', '/', '(',	/* 9 */
    ')', '=', '-', '=', BACKSPACE,	/* Backspace */
    '\t',			/* Tab */
    'Q', 'W', 'E', 'R',	/* 19 */
    'T', 'Y', 'U', 'I', 'O', 'P', '[', ']', ENTER,	/* Enter key */
    CONTROL,			/* 29   - Control */
    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ';',	/* 39 */
    '\'', '`', LEFT_SHIFT,		/* Left shift */
    '\\', 'Z', 'X', 'C', 'V', 'B', 'N',			/* 49 */
    'M', ',', '.', '/',   0,				/* Right shift */
    '*',
    0,	/* Alt */
    ' ',	/* Space bar */
    CAPS_LOCK,	/* Caps lock */
    0,	/* 59 - F1 key ... > */
    0,   0,   0,   0,   0,   0,   0,   0,
    0,	/* < ... F10 */
    0,	/* 69 - Num lock*/
    0,	/* Scroll Lock */
    0,	/* Home key */
    ARROW_UP,	/* Up Arrow */
    PAGE_UP,	/* Page Up */
    '-',
    ARROW_LEFT,	/* Left Arrow */
    0,
    ARROW_RIGHT,	/* Right Arrow */
    '+',
    0,	/* 79 - End key*/
    ARROW_DOWN,	/* Down Arrow */
    PAGE_DOWN,	/* Page Down */
    0,	/* Insert Key */
    0,	/* Delete Key */
    0,   0,   0,
    0,	/* F11 Key */
    0,	/* F12 Key */
    0,	/* All other keys are undefined */
};

__attribute__((interrupt)) void keyboard_interrupt(void* frame)
{        
    uint8_t scanCode = io_inb(0x60);

    if (!(scanCode & (0b10000000))) //If key was pressed down
    {
        tty_put(SCAN_CODE_TABLE[scanCode]);
    }

    io_outb(PIC1_COMMAND, PIC_EOI);
}
/*
__attribute__((interrupt)) void syscall_interrupt(void* frame)
{       
    uint64_t rax;
    asm volatile("movq %%rax, %0" : "=r" (rax));
    uint64_t rdi;
    asm volatile("movq %%rdi, %0" : "=r" (rdi));
    uint64_t rsi;
    asm volatile("movq %%rsi, %0" : "=r" (rsi));
    uint64_t rdx;
    asm volatile("movq %%rdx, %0" : "=r" (rdx));

    rax = syscall(rax, rdi, rsi, rdx);
    asm volatile("movq %0, %%rax" : : "r"(rax));
    
    io_outb(PIC1_COMMAND, PIC_EOI);
}*/