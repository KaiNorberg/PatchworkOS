#pragma once

#include <stdint.h>
#include <stddef.h>

#include <efi.h>
#include <efilib.h>

size_t strlen(const char* str);
char* strcpy(char* dest, const char* src);
int strcmp(const char* str1, const char* str2);

void char16_to_char(CHAR16* string, char* out);