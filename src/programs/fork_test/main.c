#include <stdint.h>

#include "stdlib.h"

#include "../../common.h"

extern uint64_t sys_fork();

void sys_test(const char* string)
{
    uint64_t rax = SYS_TEST; //SYS_TEST
    uint64_t rdi = (uint64_t)string;
    asm volatile("movq %0, %%rax;" "movq %1, %%rdi;" "int $0x80": : "r"(rax), "r"(rdi));    
}

int main(int argc, char* argv[])
{   
    sys_test("Hello from parent, forking...\r\n\n");
    
    uint64_t pid = sys_fork();

    if (pid == 0)
    {
        while (1)
        {
            sys_test("Hello from child program!     \r");
        }            
    }
    else
    {
        while (1)
        {
            sys_test("Hello from parent program!    \r");
        }       
    }       

    return 0;
}