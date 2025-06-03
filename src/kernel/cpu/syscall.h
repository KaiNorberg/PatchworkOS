#pragma once

#include "trap.h"

typedef struct
{
    uint64_t kernelRsp;
    uint64_t userRsp;
    bool inSyscall;
} syscall_ctx_t;

extern void syscall_entry(void);

void syscall_ctx_init(syscall_ctx_t* ctx, uint64_t kernelRsp);

void syscall_ctx_load(syscall_ctx_t* ctx);

void syscall_init(void);

void syscall_handler(trap_frame_t* trapFrame);