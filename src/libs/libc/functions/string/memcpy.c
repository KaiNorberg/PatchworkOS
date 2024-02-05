#include <libc/string.h>

void* memcpy(void* dest, const void* src, const uint64_t count)
{	
    for (uint64_t i = 0; i < count / sizeof(uint64_t); i++)
    {
        ((uint64_t*)dest)[i] = ((const uint64_t*)src)[i];
    }

    for (uint64_t i = 0; i < count % sizeof(uint64_t); i++)
    {
        ((uint8_t*)dest)[count / sizeof(uint64_t) + i] = ((const uint8_t*)src)[count / sizeof(uint64_t) + i];
    }

    return dest;
}