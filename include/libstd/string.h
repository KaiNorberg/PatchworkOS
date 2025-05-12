#ifndef _STRING_H
#define _STRING_H 1

#if defined(__cplusplus)
extern "C"
{
#endif

#include "_AUX/NULL.h"
#include "_AUX/config.h"
#include "_AUX/size_t.h"

_PUBLIC void* memcpy(void* _RESTRICT s1, const void* _RESTRICT s2, size_t n);

_PUBLIC void* memmove(void* _RESTRICT s1, const void* _RESTRICT s2, size_t n);

_PUBLIC char* strcpy(char* _RESTRICT s1, const char* _RESTRICT s2);

_PUBLIC char* strncpy(char* _RESTRICT s1, const char* _RESTRICT s2, size_t n);

_PUBLIC char* strcat(char* _RESTRICT s1, const char* _RESTRICT s2);

_PUBLIC char* strncat(char* _RESTRICT s1, const char* _RESTRICT s2, size_t n);

_PUBLIC int memcmp(const void* s1, const void* s2, size_t n);

_PUBLIC int strcmp(const char* s1, const char* s2);

_PUBLIC int strcoll(const char* s1, const char* s2);

_PUBLIC int strncmp(const char* s1, const char* s2, size_t n);

_PUBLIC size_t strxfrm(char* _RESTRICT s1, const char* _RESTRICT s2, size_t n);

_PUBLIC void* memchr(const void* s, int c, size_t n);

_PUBLIC char* strchr(const char* s, int c);

_PUBLIC size_t strcspn(const char* s1, const char* s2);

_PUBLIC char* strpbrk(const char* s1, const char* s2);

_PUBLIC char* strrchr(const char* s, int c);

_PUBLIC size_t strspn(const char* s1, const char* s2);

_PUBLIC char* strstr(const char* s1, const char* s2);

_PUBLIC char* strtok(char* _RESTRICT s1, const char* _RESTRICT s2);

_PUBLIC void* memset(void* s, int c, size_t n);
_PUBLIC void* memset32(void* s, __UINT32_TYPE__ c, size_t n);

_PUBLIC char* strerror(int errnum);

_PUBLIC size_t strlen(const char* s);

// Note: Technically this should not be here as we are using C11, and this is only available in C23, but its just to
// useful to leave out.
char* strdup(const char* src);

#if (__STDC_WANT_LIB_EXT1__ + 0) != 0

#include "_AUX/errno_t.h"
#include "_AUX/rsize_t.h"

_PUBLIC errno_t memcpy_s(void* _RESTRICT s1, rsize_t s1max, const void* _RESTRICT s2, rsize_t n);

_PUBLIC errno_t memmove_s(void* _RESTRICT s1, rsize_t s1max, const void* _RESTRICT s2, rsize_t n);

_PUBLIC errno_t strcpy_s(char* _RESTRICT s1, rsize_t s1max, const char* _RESTRICT s2);

_PUBLIC errno_t strncpy_s(char* _RESTRICT s1, rsize_t s1max, const char* _RESTRICT s2, rsize_t n);

_PUBLIC errno_t strcat_s(char* _RESTRICT s1, rsize_t s1max, const char* _RESTRICT s2);

_PUBLIC errno_t strncat_s(char* _RESTRICT s1, rsize_t s1max, const char* _RESTRICT s2, rsize_t n);

_PUBLIC char* strtok_s(char* _RESTRICT s1, rsize_t* _RESTRICT s1max, const char* _RESTRICT s2, char** _RESTRICT ptr);

_PUBLIC errno_t memset_s(void* s, rsize_t smax, int c, rsize_t n);

_PUBLIC errno_t strerror_s(char* s, rsize_t maxsize, errno_t errnum);

_PUBLIC size_t strerrorlen_s(errno_t errnum);

_PUBLIC size_t strnlen_s(const char* s, size_t maxsize);

#endif

#if defined(__cplusplus)
}
#endif

#endif
