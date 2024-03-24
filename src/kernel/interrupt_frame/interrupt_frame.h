#pragma once

#include "types/types.h"

typedef struct __attribute__((packed))
{
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
} InterruptFrame;

void interrupt_frame_copy(InterruptFrame* dest, InterruptFrame const* src);
