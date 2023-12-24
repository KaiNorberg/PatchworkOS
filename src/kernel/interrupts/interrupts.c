#include "interrupts.h"

#include "kernel/kernel.h"
#include "io/io.h"
#include "tty/tty.h"
#include "utils/utils.h"
#include "syscall/syscall.h"
#include "scheduler/scheduler.h"
#include "debug/debug.h"
#include "string/string.h"
#include "time/time.h"
#include "heap/heap.h"

#include "../common.h"

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

void interrupts_init()
{
    tty_start_message("Interrupts initializing");

    tty_end_message(TTY_MESSAGE_OK);
}

void interrupt_handler(InterruptStackFrame* stackFrame)
{       
    if (stackFrame->vector < 32) //Exception
    {
        exception_handler(stackFrame);
    }
    else if (stackFrame->vector >= 32 && stackFrame->vector <= 48) //IRQ
    {    
        irq_handler(stackFrame);
    }
    else if (stackFrame->vector == 0x80) //Syscall
    {
        syscall_handler(stackFrame);
    }  
}

void irq_handler(InterruptStackFrame* stackFrame)
{
    uint64_t irq = stackFrame->vector - 32;

    switch (irq)
    {
    case IRQ_PIT:
    {
        time_tick();

        if (time_get_tick() % (TICKS_PER_SECOND / 2) == 0) //For testing
        {
            context_save(scheduler_get_running_process()->context, stackFrame);
            scheduler_schedule();    
            context_load(scheduler_get_running_process()->context, stackFrame);
        }
    }
    break;
    default:
    {
        //Not implemented
    }
    break;
    }        

    io_pic_eoi(irq); 
}

void exception_handler(InterruptStackFrame* stackFrame)
{
    uint64_t randomNumber = 0;

    Pixel black;
    black.a = 255;
    black.r = 0;
    black.g = 0;
    black.b = 0;

    Pixel white;
    white.a = 255;
    white.r = 255;
    white.g = 255;
    white.b = 255;

    Pixel red;
    red.a = 255;
    red.r = 224;
    red.g = 108;
    red.b = 117;

    uint64_t scale = 3;

    Point startPoint;
    startPoint.x = 100;
    startPoint.y = 50;

    tty_set_scale(scale);

    //tty_clear();

    tty_set_background(black);
    tty_set_foreground(white);

    tty_set_cursor_pos(startPoint.x, startPoint.y);
    tty_print("KERNEL PANIC!\n\r");

    tty_set_cursor_pos(startPoint.x, startPoint.y + 16 * 1 * scale);
    tty_print("// ");
    tty_print(errorJokes[randomNumber]);

    tty_set_background(black);
    tty_set_foreground(red);

    tty_set_cursor_pos(startPoint.x, startPoint.y + 16 * 3 * scale);
    tty_print("\"");
    tty_print(exception_strings[stackFrame->vector]);
    tty_print("\": ");
    for (int i = 0; i < 32; i++)
    {
        tty_put('0' + ((stackFrame->errorCode >> (i)) & 1));
    }

    tty_set_background(black);
    tty_set_foreground(white);

    tty_set_cursor_pos(startPoint.x, startPoint.y + 16 * 5 * scale);
    tty_print("OS_VERSION = ");
    tty_print(OS_VERSION);

    tty_set_cursor_pos(startPoint.x, startPoint.y + 16 * 7 * scale);
    tty_print("Interrupt Stack Frame: ");

    tty_set_cursor_pos(startPoint.x, startPoint.y + 16 * 8 * scale);
    tty_print("Instruction pointer = ");
    tty_printx(stackFrame->instructionPointer);

    tty_set_cursor_pos(startPoint.x, startPoint.y + 16 * 9 * scale);
    tty_print("Code segment = ");
    tty_printx(stackFrame->codeSegment);

    tty_set_cursor_pos(startPoint.x, startPoint.y + 16 * 10 * scale);
    tty_print("Rflags = ");
    tty_printx(stackFrame->flags);

    tty_set_cursor_pos(startPoint.x, startPoint.y + 16 * 11 * scale);
    tty_print("Stack pointer = ");
    tty_printx(stackFrame->stackPointer);

    tty_set_cursor_pos(startPoint.x, startPoint.y + 16 * 12 * scale);
    tty_print("Stack segment = ");
    tty_printx(stackFrame->stackSegment);

    tty_set_cursor_pos(startPoint.x, startPoint.y + 16 * 14 * scale);
    tty_print("Memory: ");

    tty_set_cursor_pos(startPoint.x, startPoint.y + 16 * 15 * scale);
    tty_print("Used Heap = ");
    tty_printi(heap_reserved_size());
    tty_print(" B");

    tty_set_cursor_pos(startPoint.x, startPoint.y + 16 * 16 * scale);
    tty_print("Free Heap = ");
    tty_printi(heap_free_size());
    tty_print(" B");

    tty_set_cursor_pos(startPoint.x, startPoint.y + 16 * 17 * scale);
    tty_print("Locked Pages = ");
    tty_printi(page_allocator_get_locked_amount());

    tty_set_cursor_pos(startPoint.x, startPoint.y + 16 * 18 * scale);
    tty_print("Unlocked Pages = ");
    tty_printi(page_allocator_get_unlocked_amount());

    tty_set_cursor_pos(startPoint.x, startPoint.y + 16 * 20 * scale);
    tty_print("Please manually reboot your machine.");

    while (1)
    {
        asm volatile("hlt");
    }
}