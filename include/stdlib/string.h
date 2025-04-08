#ifndef _STRING_H
#define _STRING_H 1

#if defined(__cplusplus)
extern "C"
{
#endif

#include "_AUX/NULL.h"
#include "_AUX/config.h"
#include "_AUX/size_t.h"

void* memcpy(void* _RESTRICT dest, const void* _RESTRICT src, size_t count);
void* memmove(void* dest, const void* src, size_t count);
char* strcpy(char* _RESTRICT dest, const char* _RESTRICT src);
char* strncpy(char* _RESTRICT dest, const char* _RESTRICT src, size_t count);
char* strcat(char* _RESTRICT dest, const char* _RESTRICT src);
size_t strlen(const char* str);
size_t strnlen(const char* str, size_t max);

int memcmp(const void* a, const void* b, size_t count);
int strcmp(const char* a, const char* b);
int strncmp(const char* a, const char* b, size_t count);

char* strchr(const char* str, int ch);
char* strrchr(const char* str, int ch);
char* strstr(const char* a, const char* b);

void* memset(void* dest, int ch, size_t count);
size_t strlen(const char* str);
char* strerror(int error);

#if defined(__cplusplus)
}
#endif

#endif
