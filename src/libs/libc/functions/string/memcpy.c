#include <libc/string.h>
#include <stdint.h>

void* memcpy(void* dest, const void* src, const uint64_t count)
{	
    uint64_t qwordCount = count / 8;
    uint64_t* qwordDest = (uint64_t*)dest;
    uint64_t* qwordSrc = (uint64_t*)src;

    for (uint64_t i = 0; i < qwordCount; i++) 
    {
        qwordDest[i] = qwordSrc[i];
    }

    uint64_t byteCount = count % 8;
    uint8_t* byteDest = ((uint8_t*)dest) + qwordCount * 8;
    uint8_t* byteSrc = ((uint8_t*)src) + qwordCount * 8;

    for (uint64_t i = 0; i < byteCount; i++) 
    {
        byteDest[i] = byteSrc[i];
    }

    return dest;
}