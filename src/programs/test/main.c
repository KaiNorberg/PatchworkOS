#include <stdint.h>

void _start()
{    
    uint64_t rax = 0; //Test
    asm volatile("movq %0, %%rax" : : "r"(rax));
    asm volatile("int $0x80");

    rax = 2; //Exit
    asm volatile("movq %0, %%rax" : : "r"(rax));
    asm volatile("int $0x80");
}