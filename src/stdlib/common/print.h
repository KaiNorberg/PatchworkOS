#pragma once

#include <stdarg.h>
#include <stdint.h>

uint32_t _Print(void (*putFunc)(char, void*), void* context, const char* format, va_list args);
