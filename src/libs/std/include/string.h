#ifndef _STRING_H
#define _STRING_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#include "_AUX/config.h"
#include "_AUX/size_t.h"
#include "_AUX/NULL.h"

_PUBLIC void* memcpy(void* _RESTRICT dest, const void* _RESTRICT src, size_t count);
_PUBLIC void* memmove(void* dest, const void* src, size_t count);
_PUBLIC char* strcpy(char* _RESTRICT dest, const char* _RESTRICT src);
_PUBLIC char* strncpy(char* _RESTRICT dest, const char* _RESTRICT src, size_t count);
_PUBLIC char* strcat(char* _RESTRICT dest, const char* _RESTRICT src);

_PUBLIC int memcmp(const void* a, const void* b, size_t count);
_PUBLIC int strcmp(const char* a, const char* b);

_PUBLIC char* strchr(const char* str, int ch);
_PUBLIC char* strrchr(const char* str, int ch);

_PUBLIC void* memset(void* dest, int ch, size_t count);
_PUBLIC size_t strlen(const char* str);
_PUBLIC char* strerror(int error);

#if defined(__cplusplus)
}
#endif
 
#endif