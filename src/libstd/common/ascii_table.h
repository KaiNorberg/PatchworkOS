#pragma once

#include <stdint.h>

typedef enum
{
    _ASCII_ALPHA = (1 << 0),
    _ASCII_DIGIT = (1 << 1),
    _ASCII_XDIGIT = (1 << 2),
    _ASCII_BLANK = (1 << 3),
    _ASCII_CNTRL = (1 << 4),
    _ASCII_GRAPH = (1 << 5),
    _ASCII_PUNCT = (1 << 6),
    _ASCII_SPACE = (1 << 7),
    _ASCII_LOWER = (1 << 8),
    _ASCII_UPPER = (1 << 9),
} _ascii_flags_t;

typedef struct
{
    _ascii_flags_t flags;
    unsigned char upper;
    unsigned char lower;
} _ascii_entry_t;

extern _ascii_entry_t _ascii_table[];
