#include <stdint.h>

#include "stdlib.h"
int main(int argc, char* argv[])
{               
    uint64_t rdi = 1;
    uint64_t rax = 0; //SYS_TEST
    
    asm volatile("movq %0, %%rax;" "movq %1, %%rdi;" "int $0x80": : "r"(rax), "r"(rdi));

    return 0;
}