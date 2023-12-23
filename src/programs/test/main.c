#include <stdint.h>

#include "stdlib.h"

#include "../../common.h"

void sys_test(const char* string)
{
    uint64_t rax = SYS_TEST; //SYS_TEST
    uint64_t rdi = (uint64_t)string;
    asm volatile("movq %0, %%rax;" "movq %1, %%rdi;" "int $0x80": : "r"(rax), "r"(rdi));    
}

int main(int argc, char* argv[])
{   
    while (1)
    {
        sys_test("Hello from test program!      \r");
    }            

    return 0;
}