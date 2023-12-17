#pragma once

#include <stdint.h>

typedef struct __attribute__((packed))
{
    uint64_t cr3;
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rbp;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rbx;
    uint64_t rax;

    uint64_t vector;
    uint64_t errorCode;

    uint64_t instructionPointer;
    uint64_t codeSegment;
    uint64_t flags;
    uint64_t stackPointer;
    uint64_t stackSegment;
} InterruptStackFrame;

enum 
{
    IRQ_PIT = 0,
    IRQ_KEYBOARD = 1,
    IRQ_CASCADE = 2,
    IRQ_COM2 = 3,
    IRQ_COM1 = 4,
    IRQ_LPT2 = 5,
    IRQ_FLOPPY = 6,
    IRQ_LPT1 = 7,
    IRQ_CMOS = 8,
    IRQ_FREE1 = 9,
    IRQ_FREE2 = 10,
    IRQ_FREE3 = 11,
    IRQ_PS2_MOUSE = 12,
    IRQ_FPU = 13,
    IRQ_PRIMARY_ATA_HARD_DISK = 14,
    IRQ_SECONDARY_ATA_HARD_DISK = 15
};

void interrupt_handler(InterruptStackFrame* stackFrame);

void irq_handler(InterruptStackFrame* stackFrame);

void exception_handler(InterruptStackFrame* stackFrame);