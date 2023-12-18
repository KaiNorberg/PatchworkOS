#include "interrupt_stack.h"

#include <stdint.h>

__attribute__((aligned(0x1000)))
uint8_t interruptStack[0x1000];

void* interrupt_stack_get()
{
    return (void*)interruptStack;
}
