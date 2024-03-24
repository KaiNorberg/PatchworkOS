#include "debug.h"

#include "tty/tty.h"
#include "heap/heap.h"
#include "pmm/pmm.h"
#include "time/time.h"
#include "hpet/hpet.h"
#include "registers/registers.h"
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

static inline void debug_set_x(int8_t x)
{    
    xPos = x;
    tty_set_column((tty_column_amount() - DEBUG_COLUMN_AMOUNT * DEBUG_COLUMN_WIDTH) / 2 + x * DEBUG_COLUMN_WIDTH);
}

static inline void debug_set_y(int8_t y)
{    
    yPos = y;
    tty_set_row((tty_row_amount() - DEBUG_ROW_AMOUNT) / 2 + y);
}

static inline void debug_start(const char* message)
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

static inline void debug_move(const char* name, uint8_t x, uint8_t y)
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

static inline void debug_print(const char* string, uint64_t value)
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

    smp_send_ipi_to_others(IPI_HALT);

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

    while (true)
    {
        asm volatile("hlt");
    }
}

void debug_exception(InterruptFrame const* interruptFrame, const char* message)
{
    tty_acquire();

    smp_send_ipi_to_others(IPI_HALT);

    uint32_t oldRow = tty_get_row();
    uint32_t oldColumn = tty_get_column();

    debug_start(message);

    debug_move("Interrupt Frame", 0, 0);
    if (interruptFrame != NULL)
    {
        debug_print("Vector = ", interruptFrame->vector);
        debug_print("Error Code = ", interruptFrame->errorCode);
        debug_print("Instruction Pointer = ", interruptFrame->instructionPointer);
        debug_print("Stack Pointer = ", interruptFrame->stackPointer);
        debug_print("RFLAGS = ", interruptFrame->flags);
        debug_print("Code Segment = ", interruptFrame->codeSegment);
        debug_print("Stack Segment = ", interruptFrame->stackSegment);

        uint64_t cr2 = cr2_read();
        uint64_t cr3 = cr3_read();
        uint64_t cr4 = cr4_read();

        debug_move("Registers", 2, 0);
        debug_print("R9 = ", interruptFrame->r9);
        debug_print("R8 = ", interruptFrame->r8);
        debug_print("RBP = ", interruptFrame->rbp);
        debug_print("RDI = ", interruptFrame->rdi);
        debug_print("RSI = ", interruptFrame->rsi);
        debug_print("RDX = ", interruptFrame->rdx);
        debug_print("RCX = ", interruptFrame->rcx);
        debug_print("RBX = ", interruptFrame->rbx);
        debug_print("RAX = ", interruptFrame->rax);

        debug_move(NULL, 3, 0);
        debug_print("CR2 = ", cr2);
        debug_print("CR3 = ", cr3);
        debug_print("CR4 = ", cr4);
        debug_print("R15 = ", interruptFrame->r15);
        debug_print("R14 = ", interruptFrame->r14);
        debug_print("R13 = ", interruptFrame->r13);
        debug_print("R12 = ", interruptFrame->r12);
        debug_print("R11 = ", interruptFrame->r11);
        debug_print("R10 = ", interruptFrame->r10);
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

    asm volatile("cli");
    while (true)
    {
        asm volatile("hlt");
    }
}