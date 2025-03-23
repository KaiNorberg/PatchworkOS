#include <errno.h>
#include <stdint.h>
#include <string.h>

void* memcpy(void* _RESTRICT dest, const void* _RESTRICT src, size_t count)
{
    uint8_t* d = dest;
    const uint8_t* s = src;

    while (((uintptr_t)d & 7) && count)
    {
        *d++ = *s++;
        count--;
    }

    while (count >= 64)
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
        count -= 64;
    }

    while (count >= 8)
    {
        *(uint64_t*)d = *(const uint64_t*)s;
        d += 8;
        s += 8;
        count -= 8;
    }

    while (count--)
    {
        *d++ = *s++;
    }

    return dest;
}

void* memmove(void* dest, const void* src, size_t count)
{
    char* p1 = (char*)src;
    char* p2 = (char*)dest;

    if (p2 <= p1)
    {
        while (count--)
        {
            *p2++ = *p1++;
        }
    }
    else
    {
        p1 += count;
        p2 += count;

        while (count--)
        {
            *--p2 = *--p1;
        }
    }

    return dest;
}

char* strcpy(char* _RESTRICT dest, const char* _RESTRICT src)
{
    char* temp = dest;

    while ((*dest++ = *src++))
    {
    }

    return temp;
}

char* strncpy(char* _RESTRICT dest, const char* _RESTRICT src, size_t count)
{
    char* ret = dest;

    while (count && (*dest++ = *src++))
    {
        --count;
    }

    while (count-- > 1)
    {
        *dest++ = '\0';
    }

    return ret;
}

char* strcat(char* _RESTRICT dest, const char* _RESTRICT src)
{
    char* ret = dest;

    if (*dest)
    {
        while (*++dest)
        {
            /* EMPTY */
        }
    }

    while ((*dest++ = *src++))
    {
        /* EMPTY */
    }

    return ret;
}

size_t strlen(const char* str)
{
    size_t i = 0;

    while (str[i])
    {
        i++;
    }

    return i;
}

size_t strnlen(const char* str, size_t max)
{
    size_t i = 0;

    while (i < max && str[i])
    {
        i++;
    }

    return i;
}

int memcmp(const void* a, const void* b, size_t count)
{
    unsigned char* p1 = (unsigned char*)a;
    unsigned char* p2 = (unsigned char*)b;

    while (count--)
    {
        if (*p1 != *p2)
        {
            return *p1 - *p2;
        }

        ++p1;
        ++p2;
    }

    return 0;
}

int strcmp(const char* a, const char* b)
{
    while ((*a) && (*a == *b))
    {
        ++a;
        ++b;
    }

    return (*((unsigned char*)a) - *((unsigned char*)b));
}

char* strchr(const char* str, int ch)
{
    do
    {
        if (*str == (char)ch)
        {
            return (char*)str;
        }
    } while (*str++);

    return NULL;
}

char* strrchr(const char* str, int ch)
{
    size_t i = 0;

    while (str[i++])
    {
        /* EMPTY */
    }

    do
    {
        if (str[--i] == (char)ch)
        {
            return (char*)str + i;
        }
    } while (i);

    return NULL;
}

void* memset(void* dest, int ch, size_t count)
{
    uint8_t* p = dest;

    uint8_t ch8 = (uint8_t)ch;
    uint64_t ch64 = ((uint64_t)ch8) | ((uint64_t)ch8 << 8) | ((uint64_t)ch8 << 16) | ((uint64_t)ch8 << 24) |
        ((uint64_t)ch8 << 32) | ((uint64_t)ch8 << 40) | ((uint64_t)ch8 << 48) | ((uint64_t)ch8 << 56);

    while (((uintptr_t)p & 7) && count)
    {
        *p++ = ch8;
        count--;
    }

    while (count >= 64)
    {
        *(uint64_t*)(p + 0) = ch64;
        *(uint64_t*)(p + 8) = ch64;
        *(uint64_t*)(p + 16) = ch64;
        *(uint64_t*)(p + 24) = ch64;
        *(uint64_t*)(p + 32) = ch64;
        *(uint64_t*)(p + 40) = ch64;
        *(uint64_t*)(p + 48) = ch64;
        *(uint64_t*)(p + 56) = ch64;
        p += 64;
        count -= 64;
    }

    while (count >= 8)
    {
        *(uint64_t*)p = ch64;
        p += 8;
        count -= 8;
    }

    while (count--)
    {
        *p++ = ch8;
    }

    return dest;
}

static char* errorStrings[] = {
    "no error",
    "math argument out of domain",
    "math result not representable",
    "illegal byte sequence",
    "not implemented",
    "bad address",
    "already exists",
    "invalid letter",
    "invalid path",
    "too many open files",
    "bad file descriptor",
    "permission denied",
    "bad executable",
    "out of memory",
    "bad request",
    "bad flag/flags",
    "invalid argument",
    "bad buffer",
    "not a directory",
    "is a directory",
    "no such resource",
    "broken pipe",
    "blocker limit exceeded",
    "busy",
};

char* strerror(int error)
{
    if (error > EBUSY || error < 0)
    {
        return "unknown error";
    }

    return errorStrings[error];
}
