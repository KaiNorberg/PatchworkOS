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
    case SYS_TEST:
    {
        tty_print("Syscall test!\n\r");
        return 0;  
    }
    break;
    default:
    {
        return -1;
    }
    break;
    }
}

