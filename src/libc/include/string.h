#pragma once

#include <stdint.h>
#include <stddef.h>

int memcmp(const void* lhs, const void* rhs, size_t count);
void* memcpy(void* dest, const void* src, size_t count);
void* memmove(void* dest, const void* src, size_t count);
void* memset(void* dest, int ch, size_t count);

size_t strlen(const char *str);
char* strcpy(char* dest, const char* src);