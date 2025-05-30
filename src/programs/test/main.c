#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

void cause_overflow() 
{
    volatile uint64_t x;
    cause_overflow();
}

int main() {
    printf("Attempting to cause a stack overflow...\n");
    cause_overflow();
    printf("We should not have reached this!\n");
    return 0;
}