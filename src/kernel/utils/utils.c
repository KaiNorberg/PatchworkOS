#include "utils.h"

#include "string/string.h"

uint64_t read_msr(uint64_t msr)
{
    uint32_t low;
    uint32_t high;
    asm volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((uint64_t)high << 32) | low;
}

void write_msr(uint64_t msr, uint64_t value)
{
    uint32_t low = value & 0xFFFFFFFF;
    uint32_t high = value >> 32;
    asm volatile("wrmsr" : : "c"(msr), "a"(low), "d"(high));
}

char* itoa(uint64_t i, char b[], uint8_t base)
{
    char* p = b;

    uint64_t shifter = i;
    do
    {
        ++p;
        shifter = shifter/base;
    }
    while(shifter);

    *p = '\0';
    do
    {
        uint8_t digit = i % base;

        *--p = digit < 10 ? '0' + digit : 'A' + digit - 10;
        i = i/base;
    }
    while(i);

    return b;
}

uint64_t stoi(const char* string) 
{
    uint64_t multiplier = 1;
    uint64_t result = 0;
    for (int64_t i = strlen(string) - 1; i >= 0; i--) 
    {
        result += multiplier * (string[i] - '0');
        multiplier *= 10;
    }
    return result;
}

uint64_t round_up(uint64_t number, uint64_t multiple)
{
    return ((number + multiple - 1) / multiple) * multiple;
}

uint64_t round_down(uint64_t number, uint64_t multiple)
{
    return (number / multiple) * multiple;
}