#ifndef _STRING_H
#define _STRING_H 1

#if defined(__cplusplus)
extern "C"
{
#endif

#include "_libstd/NULL.h"
#include "_libstd/config.h"
#include "_libstd/size_t.h"

void* memcpy(void* _RESTRICT s1, const void* _RESTRICT s2, size_t n);

void* memmove(void* _RESTRICT s1, const void* _RESTRICT s2, size_t n);

char* strcpy(char* _RESTRICT s1, const char* _RESTRICT s2);

char* strncpy(char* _RESTRICT s1, const char* _RESTRICT s2, size_t n);

char* strcat(char* _RESTRICT s1, const char* _RESTRICT s2);

char* strncat(char* _RESTRICT s1, const char* _RESTRICT s2, size_t n);

int memcmp(const void* s1, const void* s2, size_t n);

int strcmp(const char* s1, const char* s2);

int strcoll(const char* s1, const char* s2);

int strncmp(const char* s1, const char* s2, size_t n);

size_t strxfrm(char* _RESTRICT s1, const char* _RESTRICT s2, size_t n);

void* memchr(const void* s, int c, size_t n);

char* strchr(const char* s, int c);

size_t strcspn(const char* s1, const char* s2);

char* strpbrk(const char* s1, const char* s2);

char* strrchr(const char* s, int c);

size_t strspn(const char* s1, const char* s2);

char* strstr(const char* s1, const char* s2);

char* strtok(char* _RESTRICT s1, const char* _RESTRICT s2);

void* memset(void* s, int c, size_t n);
void* memset32(void* s, __UINT32_TYPE__ c, size_t n);

char* strerror(int errnum);

size_t strlen(const char* s);

// Note: Technically this should not be here as we are using C11, and this is only available in C23, but its just to
// useful to leave out.
char* strdup(const char* src);

#if (__STDC_WANT_LIB_EXT1__ + 0) != 0

#include "_libstd/errno_t.h"
#include "_libstd/rsize_t.h"

errno_t memcpy_s(void* _RESTRICT s1, rsize_t s1max, const void* _RESTRICT s2, rsize_t n);

errno_t memmove_s(void* _RESTRICT s1, rsize_t s1max, const void* _RESTRICT s2, rsize_t n);

errno_t strcpy_s(char* _RESTRICT s1, rsize_t s1max, const char* _RESTRICT s2);

errno_t strncpy_s(char* _RESTRICT s1, rsize_t s1max, const char* _RESTRICT s2, rsize_t n);

errno_t strcat_s(char* _RESTRICT s1, rsize_t s1max, const char* _RESTRICT s2);

errno_t strncat_s(char* _RESTRICT s1, rsize_t s1max, const char* _RESTRICT s2, rsize_t n);

char* strtok_s(char* _RESTRICT s1, rsize_t* _RESTRICT s1max, const char* _RESTRICT s2, char** _RESTRICT ptr);

errno_t memset_s(void* s, rsize_t smax, int c, rsize_t n);

errno_t strerror_s(char* s, rsize_t maxsize, errno_t errnum);

size_t strerrorlen_s(errno_t errnum);

size_t strnlen_s(const char* s, size_t maxsize);

#endif

#if defined(__cplusplus)
}
#endif

#endif
