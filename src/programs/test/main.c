#include <stdint.h>

#include "stdlib.h"

int main(int argc, char* argv[])
{    
    uint64_t rax = 0; //Test
    asm volatile("movq %0, %%rax" : : "r"(rax));
    asm volatile("int $0x80");
}