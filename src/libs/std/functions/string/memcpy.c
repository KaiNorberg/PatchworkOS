#include <string.h>
#include <stdint.h>

void* memcpy(void* _RESTRICT dest, const void* _RESTRICT src, size_t size)
{	
    uint64_t qwordSize = size / 8;
    uint64_t* qwordDest = (uint64_t*)dest;
    uint64_t* qwordSrc = (uint64_t*)src;

    for (uint64_t i = 0; i < qwordSize; i++) 
    {
        qwordDest[i] = qwordSrc[i];
    }

    uint64_t byteSize = size % 8;
    uint8_t* byteDest = ((uint8_t*)dest) + qwordSize * 8;
    uint8_t* byteSrc = ((uint8_t*)src) + qwordSize * 8;

    for (uint64_t i = 0; i < byteSize; i++) 
    {
        byteDest[i] = byteSrc[i];
    }

    return dest;
}