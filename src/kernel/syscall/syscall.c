#include "syscall.h"

#include "kernel/tty/tty.h"
#include "kernel/file_system/file_system.h"

#include "common.h"

uint64_t syscall_handler()
{    
    uint64_t rax;
    asm volatile("movq %%rax, %0" : "=r" (rax));
    uint64_t rdi;
    asm volatile("movq %%rdi, %0" : "=r" (rdi));
    uint64_t rsi;
    asm volatile("movq %%rsi, %0" : "=r" (rsi));
    uint64_t rdx;
    asm volatile("movq %%rdx, %0" : "=r" (rdx));

    switch (rax)
    {
    case SYS_ALLOCATE:
    {
        return 0;  
    }
    break;
    case SYS_FREE:
    {
        return 0;  
    }
    break;
    case SYS_OPEN:
    {
        return (uint64_t)file_system_get((const char*)rdi);  
    }
    break;
    }

    return -1;
}

