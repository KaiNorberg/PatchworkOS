#pragma once

#include "defs.h"

typedef struct PACKED
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

    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} trap_frame_t;

typedef struct
{
    bool intEnable;
    uint64_t depth;
} cli_ctx_t;

void cli_ctx_init(cli_ctx_t* cli);

void cli_push(void);

void cli_pop(void);

void trap_handler(trap_frame_t* trapFrame);
