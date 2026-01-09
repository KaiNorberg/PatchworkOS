#pragma once

#include <_internal/ascii.h>
#include <stdint.h>

extern const char _digits[];

extern const char _xdigits[];

extern const char _Xdigits[];

extern const char _digitPairs[];

static inline uint8_t _digit_to_int(char c)
{
    return _asciiTable[(unsigned char)c].digit;
}

static inline char _int_to_digit(uint8_t i)
{
    return _digits[i];
}

static inline char _int_to_xdigit(uint8_t i)
{
    return _xdigits[i];
}

static inline char _int_to_Xdigit(uint8_t i)
{
    return _Xdigits[i];
}
