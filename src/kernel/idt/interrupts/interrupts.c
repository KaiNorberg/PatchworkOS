#include "interrupts.h"

#include "kernel/kernel.h"

#include "io/io.h"

#include "tty/tty.h"
#include "utils/utils.h"

#include "syscall/syscall.h"
#include "multitasking/multitasking.h"

#include "debug/debug.h"

#include "string/string.h"

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

/////////////////////////////////
// Exception interrupt handlers.
/////////////////////////////////

__attribute__((interrupt)) void device_by_zero_exception(InterruptStackFrame* frame)
{    
    VIRTUAL_MEMORY_LOAD_SPACE(kernelAddressSpace);
    debug_error("Division By Zero Detected");
}

__attribute__((interrupt)) void none_maskable_interrupt_exception(InterruptStackFrame* frame)
{    
    VIRTUAL_MEMORY_LOAD_SPACE(kernelAddressSpace);
    debug_error("None Maskable Interrupt");
}

__attribute__((interrupt)) void breakpoint_exception(InterruptStackFrame* frame)
{    
    VIRTUAL_MEMORY_LOAD_SPACE(kernelAddressSpace);
    debug_error("Breakpoint reached");
}

__attribute__((interrupt)) void overflow_exception(InterruptStackFrame* frame)
{    
    VIRTUAL_MEMORY_LOAD_SPACE(kernelAddressSpace);
    debug_error("Overflow detected");
}

__attribute__((interrupt)) void boundRange_exception(InterruptStackFrame* frame)
{    
    VIRTUAL_MEMORY_LOAD_SPACE(kernelAddressSpace);
    debug_error("Bound Range Exceeded");
}

__attribute__((interrupt)) void invalid_opcode_exception(InterruptStackFrame* frame)
{    
    VIRTUAL_MEMORY_LOAD_SPACE(kernelAddressSpace);
}

__attribute__((interrupt)) void device_not_detected_exception(InterruptStackFrame* frame)
{    
    VIRTUAL_MEMORY_LOAD_SPACE(kernelAddressSpace);
    debug_error("Device Not Detected");
}

__attribute__((interrupt)) void double_fault_exception(InterruptStackFrame* frame)
{    
    VIRTUAL_MEMORY_LOAD_SPACE(kernelAddressSpace);
    debug_error("Double Fault");
}

__attribute__((interrupt)) void invalid_tts_exception(InterruptStackFrame* frame)
{    
    VIRTUAL_MEMORY_LOAD_SPACE(kernelAddressSpace);
    debug_error("Invalid TSS");
}

__attribute__((interrupt)) void segment_not_present_exception(InterruptStackFrame* frame)
{    
    VIRTUAL_MEMORY_LOAD_SPACE(kernelAddressSpace);
    debug_error("Segment Not Present");
}

__attribute__((interrupt)) void stack_segment_exception(InterruptStackFrame* frame)
{    
    VIRTUAL_MEMORY_LOAD_SPACE(kernelAddressSpace);
    debug_error("Stack Segment Fault");
}

__attribute__((interrupt)) void general_protection_exception(InterruptStackFrame* frame)
{    
    VIRTUAL_MEMORY_LOAD_SPACE(kernelAddressSpace);
    debug_error("General Protection Fault");
}

__attribute__((interrupt)) void page_fault_exception(InterruptStackFrame* frame, uint64_t errorCode)
{   
    VIRTUAL_MEMORY_LOAD_SPACE(kernelAddressSpace);

    char buffer[64];
    memset(buffer, 0, 64);

    strcpy(buffer, "Page Fault: ");
    for (int i = 0; i < 32; i++)
    {
        buffer[i + 12] = '0' + ((errorCode >> (i)) & 1);
    }
    
    debug_error(buffer);
}

__attribute__((interrupt)) void floating_point_exception(InterruptStackFrame* frame)
{    
    VIRTUAL_MEMORY_LOAD_SPACE(kernelAddressSpace);
    debug_error("Floating Point Exception");
}

/////////////////////////////////
// IRQ interrupt handlers.
/////////////////////////////////

__attribute__((interrupt)) void keyboard_interrupt(InterruptStackFrame* frame)
{        
    uint64_t taskAddressSpace;
    asm volatile("movq %%cr3, %0" : "=r" (taskAddressSpace));

    VIRTUAL_MEMORY_LOAD_SPACE(kernelAddressSpace);

    uint8_t scanCode = io_inb(0x60);

    if (!(scanCode & (0b10000000))) //If key was pressed down
    {
        tty_put(SCAN_CODE_TABLE[scanCode]);
    }

    io_outb(PIC1_COMMAND, PIC_EOI);

    VIRTUAL_MEMORY_LOAD_SPACE(taskAddressSpace);
}

/*__attribute__((interrupt)) void syscall_interrupt(InterruptStackFrame* frame)
{   
    tty_printi((uint64_t)frame);
}*/
