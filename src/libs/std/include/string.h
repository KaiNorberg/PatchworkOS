#ifndef _STRING_H
#define _STRING_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#include "auxiliary/config.h"
#include "auxiliary/size_t.h"
#include "auxiliary/NULL.h"

_EXPORT void* memcpy(void* _RESTRICT dest, const void* _RESTRICT src, size_t size);
_EXPORT void* memmove(void* dest, const void* src, size_t size);
_EXPORT char* strcpy(char* _RESTRICT dest, const char* src);

_EXPORT int memcmp(const void* a, const void* b, size_t size);
_EXPORT int strcmp(const char* a, const char* b);

_EXPORT char* strchr(const char* str, int ch);
_EXPORT char* strrchr(const char* str, int ch);

_EXPORT void* memset(void* dest, int ch, size_t size);
_EXPORT size_t strlen(const char* str);

#if defined(__cplusplus)
}
#endif
 
#endif