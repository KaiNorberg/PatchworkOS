#pragma once

#include <stdint.h>

typedef struct 
{
    uint64_t InstructionPointer;
    uint64_t CodeSegment;
    uint64_t Flags;
    uint64_t StackPointer;
    uint64_t StackSegment;
} InterruptStackFrame;

__attribute__((interrupt)) void keyboard_interrupt(InterruptStackFrame* frame);

__attribute__((interrupt)) void syscall_interrupt(InterruptStackFrame* frame);