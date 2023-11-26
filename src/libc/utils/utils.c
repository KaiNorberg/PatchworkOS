#include "utils.h"

uint64_t syscall_helper(uint64_t rax, uint64_t rdi, uint64_t rsi, uint64_t rdx)
{
    asm volatile("movq %0, %%rax" : : "r"(rax));
    asm volatile("movq %0, %%rdi" : : "r"(rdi));
    asm volatile("movq %0, %%rsi" : : "r"(rsi));
    asm volatile("movq %0, %%rdx" : : "r"(rdx));
    
    asm volatile("int $0x80");

    asm volatile("movq %%rax, %0" : "=r" (rax));
    return rax;
}