#ifndef _STRING_H
#define _STRING_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#include "_AUX/config.h"
#include "_AUX/size_t.h"
#include "_AUX/NULL.h"

_EXPORT void* memcpy(void* _RESTRICT dest, const void* _RESTRICT src, size_t count);
_EXPORT void* memmove(void* dest, const void* src, size_t count);
_EXPORT char* strcpy(char* _RESTRICT dest, const char* _RESTRICT src);
_EXPORT char* strncpy(char* _RESTRICT dest, const char* _RESTRICT src, size_t count);
_EXPORT char* strcat(char* _RESTRICT dest, const char* _RESTRICT src);

_EXPORT int memcmp(const void* a, const void* b, size_t count);
_EXPORT int strcmp(const char* a, const char* b);

_EXPORT char* strchr(const char* str, int ch);
_EXPORT char* strrchr(const char* str, int ch);

_EXPORT void* memset(void* dest, int ch, size_t count);
_EXPORT size_t strlen(const char* str);
_EXPORT char* strerror(int error);

#if defined(__cplusplus)
}
#endif
 
#endif