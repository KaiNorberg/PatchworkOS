#include <stdint.h>

#include "stdlib.h"

void sys_test(const char* string)
{
    uint64_t rax = 0; //SYS_TEST
    uint64_t rdi = (uint64_t)string;
    asm volatile("movq %0, %%rax;" "movq %1, %%rdi;" "int $0x80": : "r"(rax), "r"(rdi));    
}

void sys_yield()
{
    uint64_t rax = 1; //SYS_YIELD
    asm volatile("movq %0, %%rax;" "int $0x80": : "r"(rax));
}

int main(int argc, char* argv[])
{   
    while (1)
    {
        sys_test("And this is hello from program 2!\r");
    }            

    return 0;
}