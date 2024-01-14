#include "system.h"

void system_exit(int status)
{
    long int rax = SYS_EXIT; //Exit
    long int rdi = status;
    
    asm volatile("movq %0, %%rax" : : "r"(rax));
    asm volatile("movq %0, %%rdi" : : "r"(rdi));
    asm volatile("int $0x80");
}