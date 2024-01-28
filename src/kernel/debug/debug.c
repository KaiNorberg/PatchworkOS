#include "debug.h"

#include "tty/tty.h"

#include "heap/heap.h"
#include "page_allocator/page_allocator.h"
#include "time/time.h"
#include "utils/utils.h"
#include "hpet/hpet.h"

#include "../common.h"

const char* exceptionStrings[32] = 
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
};

static uint32_t currentRow;
static uint32_t currentColumn;
static Pixel currentColor;

void debug_panic(const char* message)
{
    //tty_clear();
    
    tty_acquire();

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

    debug_move_to_grid(0, 0, red);
    tty_print("KERNEL PANIC! - "); tty_print(message);

    debug_move_to_grid(2, 0, white);
    tty_print("[Time]"); 
    debug_next_row();
    tty_print("Tick = "); tty_printx(hpet_read_counter());
    debug_next_row();
    tty_print("Current Time = "); tty_printx(time_nanoseconds());

    debug_move_to_grid(2, 2, white);
    tty_print("[Memory]"); 
    debug_next_row();
    tty_print("Free Heap = "); tty_printx(heap_free_size());
    debug_next_row();
    tty_print("Reserved Heap = "); tty_printx(heap_reserved_size());
    debug_next_row();
    tty_print("Locked Pages = "); tty_printx(page_allocator_locked_amount());
    debug_next_row();
    tty_print("Unlocked Pages = "); tty_printx(page_allocator_unlocked_amount());

    while (1)
    {
        asm volatile("hlt");
    }
}

void debug_exception(InterruptFrame* interruptFrame, const char* message)
{    
    //tty_clear();

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

    debug_move_to_grid(0, 0, red);
    tty_print("KERNEL PANIC! - "); tty_print(message);

    debug_move_to_grid(2, 0, white);
    tty_print("[Interrupt Frame]"); 
    debug_next_row();
    tty_print("Exception = "); tty_print(exceptionStrings[interruptFrame->vector]);
    debug_next_row();
    tty_print("Error Code = "); tty_printx(interruptFrame->errorCode);
    debug_next_row();
    tty_print("Instruction Pointer = "); tty_printx(interruptFrame->instructionPointer);
    debug_next_row();
    tty_print("Stack Pointer = "); tty_printx(interruptFrame->stackPointer);
    debug_next_row();
    tty_print("RFLAGS = "); tty_printx(interruptFrame->flags);
    debug_next_row();
    tty_print("Code Segment = "); tty_printx(interruptFrame->codeSegment);
    debug_next_row();
    tty_print("Stack Segment = "); tty_printx(interruptFrame->stackSegment);

    uint64_t cr2;
    uint64_t cr4;
    READ_REGISTER("cr2", cr2);
    READ_REGISTER("cr4", cr4);

    debug_move_to_grid(2, 2, white);
    tty_print("[Registers]"); 
    debug_next_row();
    tty_print("CR2 = "); tty_printx(cr2);
    debug_next_row();
    tty_print("CR3 = "); tty_printx(interruptFrame->cr3);
    debug_next_row();
    tty_print("CR4 = "); tty_printx(cr4);
    debug_next_row();
    tty_print("R15 = "); tty_printx(interruptFrame->r15);
    debug_next_row();
    tty_print("R14 = "); tty_printx(interruptFrame->r14);
    debug_next_row();
    tty_print("R13 = "); tty_printx(interruptFrame->r13);
    debug_next_row();
    tty_print("R12 = "); tty_printx(interruptFrame->r12);
    debug_next_row();
    tty_print("R11 = "); tty_printx(interruptFrame->r11);
    debug_next_row();
    tty_print("R10 = "); tty_printx(interruptFrame->r10);

    debug_move_to_grid(3, 3, white);
    tty_print("R9 = "); tty_printx(interruptFrame->r9);
    debug_next_row();
    tty_print("R8 = "); tty_printx(interruptFrame->r8);
    debug_next_row();
    tty_print("RBP = "); tty_printx(interruptFrame->rbp);
    debug_next_row();
    tty_print("RDI = "); tty_printx(interruptFrame->rdi);
    debug_next_row();
    tty_print("RSI = "); tty_printx(interruptFrame->rsi);
    debug_next_row();
    tty_print("RDX = "); tty_printx(interruptFrame->rdx);
    debug_next_row();
    tty_print("RCX = "); tty_printx(interruptFrame->rcx);
    debug_next_row();
    tty_print("RBX = "); tty_printx(interruptFrame->rbx);
    debug_next_row();
    tty_print("RAX = "); tty_printx(interruptFrame->rax);

    debug_move_to_grid(13, 0, white);
    tty_print("[Time]"); 
    debug_next_row();
    tty_print("Tick = "); tty_printx(hpet_read_counter());
    debug_next_row();
    tty_print("Current Time = "); tty_printx(time_nanoseconds());

    debug_move_to_grid(13, 2, white);
    tty_print("[Memory]"); 
    debug_next_row();
    tty_print("Free Heap = "); tty_printx(heap_free_size());
    debug_next_row();
    tty_print("Reserved Heap = "); tty_printx(heap_reserved_size());
    debug_next_row();
    tty_print("Locked Pages = "); tty_printx(page_allocator_locked_amount());
    debug_next_row();
    tty_print("Unlocked Pages = "); tty_printx(page_allocator_unlocked_amount());

    tty_set_foreground(white);
    tty_set_scale(1);
}

void debug_move_to_grid(uint8_t row, uint8_t column, Pixel color)
{
    currentRow = row;
    currentColumn = column;
    currentColor = color;

    uint32_t leftPadding = (tty_get_screen_width() - DEBUG_COLUMN_AMOUNT * DEBUG_COLUMN_WIDTH * TTY_CHAR_WIDTH * DEBUG_TEXT_SCALE) / 2;
    uint32_t topPadding = (tty_get_screen_height() - DEBUG_ROW_AMOUNT * TTY_CHAR_HEIGHT * DEBUG_TEXT_SCALE) / 2;

    uint32_t xPos = leftPadding + column * DEBUG_COLUMN_WIDTH * TTY_CHAR_WIDTH * DEBUG_TEXT_SCALE;
    uint32_t yPos = topPadding + row * TTY_CHAR_HEIGHT * DEBUG_TEXT_SCALE;

    tty_set_cursor_pos(xPos, yPos);
    tty_set_scale(DEBUG_TEXT_SCALE);
    tty_set_foreground(color);
}

void debug_next_row()
{
    debug_move_to_grid(currentRow + 1, currentColumn, currentColor);
}