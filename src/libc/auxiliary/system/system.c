#include "system.h"

void system_exit(int status)
{
    long int rdi = status;
    long int rax = SYS_EXIT; //Exit
    asm volatile("movq %0, %%rax;" "movq %1, %%rdi" : : "r"(rax), "r"(rdi));
    asm volatile("int $0x80");
}