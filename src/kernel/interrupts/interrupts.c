#include "interrupts.h"

#include "kernel/kernel.h"

#include "io/io.h"

#include "tty/tty.h"
#include "utils/utils.h"

#include "syscall/syscall.h"
#include "multitasking/multitasking.h"

#include "debug/debug.h"

#include "string/string.h"

#include "../common.h"

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

const char* exception_strings[32] = 
{
    "Division Fault",
    "Debug",
    "Non-Maskable Interrupt",
    "Breakpoint",
    "Overflow",
    "Bound Range Exceeded",
    "Invalid Opcode",
    "Device Not Available",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Invalid TSS",
    "Segment Not Present",
    "Stack-Segment Fault",
    "General Protection Fault",
    "Page Fault",
    "Reserved",
    "Floating Point Exception",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating-Point Exception",
    "Virtualization Exception",
    "Control Protection Exception",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Hypervisor Injection Exception",
    "VMM Communication Exception",
    "Security Exception"
};

void interrupt_handler(InterruptStackFrame* stackFrame)
{
    if (stackFrame->Vector < 32) //Exception
    {
        exception_handler(stackFrame);
    }
    else if (stackFrame->Vector >= 32 && stackFrame->Vector <= 48) //IRQ
    {
        irq_handler(stackFrame);
    }
    else if (stackFrame->Vector == 0x80) //Syscall
    {
        syscall_handler(stackFrame);
    }
}

void irq_handler(InterruptStackFrame* stackFrame)
{
    uint64_t irq = stackFrame->Vector - 32;

    switch (irq)
    {
    case IRQ_KEYBOARD:
    {
        //Temporay code for testing
        uint8_t scanCode = io_inb(0x60);

        if (!(scanCode & (0b10000000))) //If key was pressed down
        {
            tty_put(SCAN_CODE_TABLE[scanCode]);
        }

        io_outb(PIC1_COMMAND, PIC_EOI);        
    }
    break;
    default:
    {
        //Not implemented
    }
    break;
    }
}

void exception_handler(InterruptStackFrame* stackFrame)
{
    uint64_t randomNumber = 0;

    Pixel black;
    black.A = 255;
    black.R = 0;
    black.G = 0;
    black.B = 0;

    Pixel white;
    white.A = 255;
    white.R = 255;
    white.G = 255;
    white.B = 255;

    Pixel red;
    red.A = 255;
    red.R = 224;
    red.G = 108;
    red.B = 117;

    uint64_t scale = 3;

    Point startPoint;
    startPoint.X = 100;
    startPoint.Y = 50;

    tty_set_scale(scale);

    tty_clear();

    tty_set_background(black);
    tty_set_foreground(white);

    tty_set_cursor_pos(startPoint.X, startPoint.Y);
    tty_print("KERNEL PANIC!\n\r");

    tty_set_cursor_pos(startPoint.X, startPoint.Y + 16 * 1 * scale);
    tty_print("// ");
    tty_print(errorJokes[randomNumber]);

    tty_set_background(black);
    tty_set_foreground(red);

    tty_set_cursor_pos(startPoint.X, startPoint.Y + 16 * 3 * scale);
    tty_print("\"");
    tty_print(exception_strings[stackFrame->Vector]);
    tty_print("\": ");
    for (int i = 0; i < 32; i++)
    {
        tty_put('0' + ((stackFrame->ErrorCode >> (i)) & 1));
    }

    tty_set_background(black);
    tty_set_foreground(white);

    tty_set_cursor_pos(startPoint.X, startPoint.Y + 16 * 5 * scale);
    tty_print("OS_VERSION = ");
    tty_print(OS_VERSION);

    tty_set_cursor_pos(startPoint.X, startPoint.Y + 16 * 7 * scale);
    tty_print("Interrupt Stack Frame: ");

    tty_set_cursor_pos(startPoint.X, startPoint.Y + 16 * 8 * scale);
    tty_print("Instruction pointer = ");
    tty_printx(stackFrame->InstructionPointer);

    tty_set_cursor_pos(startPoint.X, startPoint.Y + 16 * 9 * scale);
    tty_print("Code segment = ");
    tty_printx(stackFrame->CodeSegment);

    tty_set_cursor_pos(startPoint.X, startPoint.Y + 16 * 10 * scale);
    tty_print("Rflags = ");
    tty_printx(stackFrame->Flags);

    tty_set_cursor_pos(startPoint.X, startPoint.Y + 16 * 11 * scale);
    tty_print("Stack pointer = ");
    tty_printx(stackFrame->StackPointer);

    tty_set_cursor_pos(startPoint.X, startPoint.Y + 16 * 12 * scale);
    tty_print("Stack segment = ");
    tty_printx(stackFrame->StackSegment);

    tty_set_cursor_pos(startPoint.X, startPoint.Y + 16 * 14 * scale);
    tty_print("Memory: ");

    tty_set_cursor_pos(startPoint.X, startPoint.Y + 16 * 15 * scale);
    tty_print("Used Heap = ");
    tty_printi(heap_reserved_size());
    tty_print(" B");

    tty_set_cursor_pos(startPoint.X, startPoint.Y + 16 * 16 * scale);
    tty_print("Free Heap = ");
    tty_printi(heap_free_size());
    tty_print(" B");

    tty_set_cursor_pos(startPoint.X, startPoint.Y + 16 * 17 * scale);
    tty_print("Locked Pages = ");
    tty_printi(page_allocator_get_locked_amount());

    tty_set_cursor_pos(startPoint.X, startPoint.Y + 16 * 18 * scale);
    tty_print("Unlocked Pages = ");
    tty_printi(page_allocator_get_unlocked_amount());

    tty_set_cursor_pos(startPoint.X, startPoint.Y + 16 * 20 * scale);
    tty_print("Please manually reboot your machine.");

    while (1)
    {
        asm volatile("HLT");
    }
}