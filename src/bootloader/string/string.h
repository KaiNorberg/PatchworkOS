#pragma once

#include <stdint.h>
#include <stddef.h>

#include <efi.h>
#include <efilib.h>

int memcmp(const void* lhs, const void* rhs, size_t count);
void* memcpy(void* dest, const void* src, size_t count);
void* memmove(void* dest, const void* src, size_t count);

size_t strlen(const char* str);
char* strcpy(char* dest, const char* src);
int strcmp(const char* str1, const char* str2);

size_t strlen16(const CHAR16* str);
CHAR16* strcpy16(CHAR16* dest, const CHAR16* src);

const char* char16_to_char(CHAR16* string);