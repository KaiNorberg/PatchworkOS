#include <assert.h>
#include <libc/cpuid.h>
#include <stdint.h>
#include <string.h>

static void* _memcpy_no_simd(void* _RESTRICT s1, const void* _RESTRICT s2, size_t n)
{
    assert(n == 0 || (s1 != NULL && s2 != NULL));
    assert((uintptr_t)s1 + n <= (uintptr_t)s2 || (uintptr_t)s2 + n <= (uintptr_t)s1);
    assert((uintptr_t)s1 < 0x0000800000000000 || (uintptr_t)s1 >= 0xFFFF800000000000);
    assert((uintptr_t)s2 < 0x0000800000000000 || (uintptr_t)s2 >= 0xFFFF800000000000);

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
    return _memcpy_no_simd(s1, s2, n);
}

#else

// Check memcpy.s
extern void* _memcpy_sse2(void* _RESTRICT s1, const void* _RESTRICT s2, size_t n);

static void* _memcpy_select(void* _RESTRICT s1, const void* _RESTRICT s2, size_t n);
static void* (*memcpy_impl)(void* _RESTRICT, const void* _RESTRICT, size_t) = _memcpy_select;

static void* _memcpy_select(void* _RESTRICT s1, const void* _RESTRICT s2, size_t n)
{
    cpuid_instruction_sets_t sets = cpuid_detect_instruction_sets();

    if (sets & CPUID_INSTRUCTION_SET_SSE2)
    {
        memcpy_impl = _memcpy_sse2;
    }
    else
    {
        memcpy_impl = _memcpy_no_simd;
    }

    return memcpy_impl(s1, s2, n);
}

void* memcpy(void* _RESTRICT s1, const void* _RESTRICT s2, size_t n)
{
    return memcpy_impl(s1, s2, n);
}

#endif // _KERNEL_