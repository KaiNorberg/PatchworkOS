#pragma once

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

typedef enum
{
    FORMAT_MINUS = (1 << 0),
    FORMAT_PLUS = (1 << 1),
    FORMAT_ALT = (1 << 2),
    FORMAT_SPACE = (1 << 3),
    FORMAT_ZERO = (1 << 4),
    FORMAT_DONE = (1 << 5),
    FORMAT_SUPPRESSED = (1 << 22),
    FORMAT_CHAR = (1 << 6),
    FORMAT_SHORT = (1 << 7),
    FORMAT_LONG = (1 << 8),
    FORMAT_LLONG = (1 << 9),
    FORMAT_INTMAX = (1 << 10),
    FORMAT_SIZE = (1 << 11),
    FORMAT_PTRDIFF = (1 << 12),
    FORMAT_POINTER = (1 << 13),
    FORMAT_DOUBLE = (1 << 14),
    FORMAT_LDOUBLE = (1 << 15),
    FORMAT_LOWER = (1 << 16),
    FORMAT_UNSIGNED = (1 << 17),
    FORMAT_DECIMAL = (1 << 18),
    FORMAT_EXPONENT = (1 << 19),
    FORMAT_GENERIC = (1 << 20),
    FORMAT_HEXA = (1 << 21)
} _FormatFlags_t;

typedef struct
{
    int32_t base;
    _FormatFlags_t flags;
    uint64_t maxChars;
    uint64_t totalChars;
    uint64_t currentChars;
    char* buffer;
    uint64_t width;
    int64_t precision;
    FILE* stream;
    va_list arg;
} _FormatCtx_t;