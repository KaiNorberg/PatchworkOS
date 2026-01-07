#pragma once

#include <_internal/ascii.h>
#include <stdint.h>

extern const char _digits[];

extern const char _xdigits[];

static inline uint8_t _digit_to_int(char c)
{
    return _asciiTable[(unsigned char)c].digit;
}
