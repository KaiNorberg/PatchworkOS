#include <stdint.h>
#include <string.h>
#include <sys/cpuid.h>

static void* memcpy_no_simd(void* _RESTRICT s1, const void* _RESTRICT s2, size_t n)
{
    uint8_t* d = s1;
    const uint8_t* s = s2;

    while (((uintptr_t)d & 7) && n)
    {
        *d++ = *s++;
        n--;
    }

    while (n >= 64)
    {
        *(uint64_t*)(d + 0) = *(const uint64_t*)(s + 0);
        *(uint64_t*)(d + 8) = *(const uint64_t*)(s + 8);
        *(uint64_t*)(d + 16) = *(const uint64_t*)(s + 16);
        *(uint64_t*)(d + 24) = *(const uint64_t*)(s + 24);
        *(uint64_t*)(d + 32) = *(const uint64_t*)(s + 32);
        *(uint64_t*)(d + 40) = *(const uint64_t*)(s + 40);
        *(uint64_t*)(d + 48) = *(const uint64_t*)(s + 48);
        *(uint64_t*)(d + 56) = *(const uint64_t*)(s + 56);
        d += 64;
        s += 64;
        n -= 64;
    }

    while (n >= 8)
    {
        *(uint64_t*)d = *(const uint64_t*)s;
        d += 8;
        s += 8;
        n -= 8;
    }

    while (n--)
    {
        *d++ = *s++;
    }

    return s1;
}

#ifdef _KERNEL_

void* memcpy(void* _RESTRICT s1, const void* _RESTRICT s2, size_t n)
{
    return memcpy_no_simd(s1, s2, n);
}

#else

// Check memcpy.s
extern void* memcpy_sse2(void* _RESTRICT s1, const void* _RESTRICT s2, size_t n);

static void* (*memcpy_impl)(void* _RESTRICT, const void* _RESTRICT, size_t) = NULL;

void* memcpy(void* _RESTRICT s1, const void* _RESTRICT s2, size_t n)
{
    if (memcpy_impl == NULL)
    {
        cpuid_instruction_sets_t sets = cpuid_detect_instruction_sets();

        if (sets & CPUID_INSTRUCTION_SET_SSE2)
        {
            memcpy_impl = memcpy_sse2;
        }
        else
        {
            memcpy_impl = memcpy_no_simd;
        }
    }

    return memcpy_impl(s1, s2, n);
}

#endif // _KERNEL_