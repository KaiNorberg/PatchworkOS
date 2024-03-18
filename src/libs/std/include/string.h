#ifndef _STRING_H
#define _STRING_H 1

#include "auxiliary/restrict.h"
#include "auxiliary/size_t.h"

#if defined(__cplusplus)
extern "C" {
#endif

void* memcpy(void* __RESTRICT dest, const void* __RESTRICT src, size_t size);
void* memmove(void* dest, const void* src, size_t size);
char* strcpy(char* __RESTRICT dest, const char* src);

int memcmp(const void* a, const void* b, size_t size);
int strcmp(const char* a, const char* b);

void* memset(void* dest, int ch, size_t size);
size_t strlen(const char* str);

#if defined(__cplusplus)
}
#endif
 
#endif