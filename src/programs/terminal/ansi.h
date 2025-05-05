#pragma once

#include <libdwm/dwm.h>

typedef struct
{
    pixel_t foreground;
    pixel_t background;
    bool bold;
    bool italic;
    bool underline;
    bool inverse;
    bool escaped;
    bool csi;
    char parameter[32];
    uint64_t parameterIndex;
} ansi_t;

void ansi_init(ansi_t* ansi);
