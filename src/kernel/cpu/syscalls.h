#pragma once

#include "trap.h"
#include "mem/vmm.h"

#include <assert.h>
#include <kernel/syscalls.h>

typedef struct
{
    uint64_t kernelRsp;
    uint64_t userRsp;
    bool inSyscall;
} syscall_ctx_t;

typedef struct {
    uint32_t number;
    void* handler;
} syscall_descriptor_t;

extern syscall_descriptor_t _syscallTableStart[];
extern syscall_descriptor_t _syscallTableEnd[];

#define SYSCALL_DEFINE(num, returnType, ...) \
    returnType syscall_handler_##num(__VA_ARGS__); \
    const syscall_descriptor_t __syscall_##num \
    __attribute__((section(".syscall_table"))) = { \
        .number = (num), \
        .handler = (void*)syscall_handler_##num, \
    }; \
    returnType syscall_handler_##num(__VA_ARGS__)

extern void syscall_entry(void);

void syscall_ctx_init(syscall_ctx_t* ctx, uint64_t kernelRsp);

void syscall_ctx_load(syscall_ctx_t* ctx);

void syscall_table_init(void);

void syscalls_cpu_init(void);

void syscall_handler(trap_frame_t* trapFrame);

// TODO: Improve verify funcs, improve multithreading string safety. copy_to_user? copy_from_user?
bool syscall_is_pointer_valid(const void* pointer, uint64_t length);
bool syscall_is_buffer_valid(space_t* space, const void* pointer, uint64_t length);
bool syscall_is_string_valid(space_t* space, const char* string);
