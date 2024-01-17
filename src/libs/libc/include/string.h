#ifndef _STRING_H
#define _STRING_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#include "internal.h"

int memcmp(const void* lhs, const void* rhs, size_t count);

void* memcpy(void* LIBC_RESTRICT dest, const void* LIBC_RESTRICT src, size_t count);

void* memmove(void* dest, const void* src, size_t count);

void* memset(void* dest, int ch, size_t count);

char* strcpy(char* LIBC_RESTRICT dest, const char* LIBC_RESTRICT src);

size_t strlen(const char *str);

#if defined(__cplusplus)
} /* extern "C" */
#endif
 
#endif