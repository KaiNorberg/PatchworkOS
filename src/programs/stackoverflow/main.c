#include <stdio.h>
#include <stdint.h>

void stack_overflow(void)
{
    uint8_t buffer[1024];
    stack_overflow();
}

int main(void)
{
    printf("Will now cause a stack overflow...\n");
    stack_overflow();
    return 0;
}
