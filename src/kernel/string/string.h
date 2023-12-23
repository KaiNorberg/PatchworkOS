#pragma once

#include <stdint.h>
#include <stddef.h>

int memcmp(const void* lhs, const void* rhs, uint64_t count);
void* memcpy(void* dest, const void* src, uint64_t count);
void* memmove(void* dest, const void* src, uint64_t count);
void* memset(void* dest, int ch, uint64_t count);
void* memclr(void* start, uint64_t count);

uint64_t strlen(const char *str);
char* strcpy(char* dest, const char* src);