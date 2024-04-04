#include "debug.h"

#include "tty/tty.h"
#include "heap/heap.h"
#include "pmm/pmm.h"
#include "time/time.h"
#include "hpet/hpet.h"
#include "regs/regs.h"
#include "smp/smp.h"

/*const char* exceptionStrings[32] = 
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
    "Floating Point",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating-Point",
    "Virtualization",
    "Control Protection",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Hypervisor Injection",
    "VMM Communication",
    "Security"
};*/

static int8_t xPos;
static int8_t yPos;

static void debug_set_x(int8_t x)
{    
    xPos = x;
    tty_set_column((tty_column_amount() - DEBUG_COLUMN_AMOUNT * DEBUG_COLUMN_WIDTH) / 2 + x * DEBUG_COLUMN_WIDTH);
}

static void debug_set_y(int8_t y)
{    
    yPos = y;
    tty_set_row((tty_row_amount() - DEBUG_ROW_AMOUNT) / 2 + y);
}

static void debug_start(const char* message)
{ 
    Pixel red;
    red.a = 255;
    red.r = 224;
    red.g = 108;
    red.b = 117;

    Pixel white;
    white.a = 255;
    white.r = 255;
    white.g = 255;
    white.b = 255;

    tty_set_scale(DEBUG_TEXT_SCALE);

    debug_set_x(0);
    debug_set_y(-1);

    tty_set_foreground(red);

    tty_print("KERNEL PANIC - ");
    tty_print(message);

    tty_set_foreground(white);
}

static void debug_move(const char* name, uint8_t x, uint8_t y)
{
    debug_set_x(x);
    debug_set_y(y);

    if (name != NULL)
    {
        tty_put('[');
        tty_print(name);
        tty_put(']');
    }

    debug_set_x(x);
    debug_set_y(y + 1);
}

static void debug_print(const char* string, uint64_t value)
{
    tty_print(string);
    tty_printx(value);

    debug_set_x(xPos);
    debug_set_y(yPos + 1);
}

void debug_panic(const char* message)
{
    asm volatile("cli");

    tty_acquire();

    uint32_t oldRow = tty_get_row();
    uint32_t oldColumn = tty_get_column();

    debug_start(message);

    debug_move("Memory", 0, 0);
    debug_print("Free Heap = ", heap_free_size());
    debug_print("Reserved Heap = ", heap_reserved_size());
    debug_print("Locked Pages = ", pmm_reserved_amount());
    debug_print("Unlocked Pages = ", pmm_free_amount());

    debug_move("Other", 2, 0);
    debug_print("Current Time = ", time_nanoseconds());
    debug_print("Cpu id = ", smp_self()->id);

    tty_set_scale(1);
    tty_set_row(oldRow);
    tty_set_column(oldColumn);        

    tty_release();

    smp_send_ipi_to_others(IPI_HALT);
    while (true)
    {
        asm volatile("hlt");
    }
}

void debug_exception(TrapFrame const* trapFrame, const char* message)
{    
    asm volatile("cli");

    tty_acquire();

    uint32_t oldRow = tty_get_row();
    uint32_t oldColumn = tty_get_column();

    debug_start(message);

    debug_move("Trap Frame", 0, 0);
    if (trapFrame != NULL)
    {
        debug_print("Vector = ", trapFrame->vector);
        debug_print("Error Code = ", trapFrame->errorCode);
        debug_print("RIP = ", trapFrame->rip);
        debug_print("RSP = ", trapFrame->rsp);
        debug_print("RFLAGS = ", trapFrame->rflags);
        debug_print("CS = ", trapFrame->cs);
        debug_print("SS = ", trapFrame->ss);
        
        debug_move("Registers", 2, 0);
        debug_print("R9 = ", trapFrame->r9);
        debug_print("R8 = ", trapFrame->r8);
        debug_print("RBP = ", trapFrame->rbp);
        debug_print("RDI = ", trapFrame->rdi);
        debug_print("RSI = ", trapFrame->rsi);
        debug_print("RDX = ", trapFrame->rdx);
        debug_print("RCX = ", trapFrame->rcx);
        debug_print("RBX = ", trapFrame->rbx);
        debug_print("RAX = ", trapFrame->rax);

        debug_move(NULL, 3, 0);
        debug_print("CR2 = ", CR2_READ());
        debug_print("CR3 = ", CR3_READ());
        debug_print("CR4 = ", CR4_READ());
        debug_print("R15 = ", trapFrame->r15);
        debug_print("R14 = ", trapFrame->r14);
        debug_print("R13 = ", trapFrame->r13);
        debug_print("R12 = ", trapFrame->r12);
        debug_print("R11 = ", trapFrame->r11);
        debug_print("R10 = ", trapFrame->r10);
    }
    else
    {
        tty_print("Panic occurred outside of interrupt");
    }

    debug_move("Memory", 0, 13);
    debug_print("Free Heap = ", heap_free_size());
    debug_print("Reserved Heap = ", heap_reserved_size());
    debug_print("Locked Pages = ", pmm_reserved_amount());
    debug_print("Unlocked Pages = ", pmm_free_amount());

    debug_move("Other", 2, 13);
    debug_print("Current Time = ", time_nanoseconds());
    debug_print("Cpu Id = ", smp_self()->id);

    tty_set_scale(1);
    tty_set_row(oldRow);
    tty_set_column(oldColumn);        
    
    tty_release();

    smp_send_ipi_to_others(IPI_HALT);
    while (true)
    {
        asm volatile("hlt");
    }
}