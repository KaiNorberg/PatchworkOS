#include "system.h"

void system_exit(int status)
{
    long int rax = SYS_EXIT; //Exit
    asm volatile("movq %0, %%rax" : : "r"(rax));
    asm volatile("int $0x80");
}