#pragma once

#include <libpatchwork/patchwork.h>

typedef struct
{
    pixel_t foreground;
    pixel_t background;
    bool isBold;
    bool isItalic;
    bool hasUnderline;
    bool isInverse;
    bool isEscaped;
    bool isCsi;
    char parameter[32];
    uint64_t parameterIndex;
} ansi_t;

void ansi_init(ansi_t* ansi);
