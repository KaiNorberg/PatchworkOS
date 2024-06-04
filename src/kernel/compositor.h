#pragma once

#include "defs.h"
#include "lock.h"

#include <sys/win.h>

typedef struct
{
    win_info_t info;
    uint32_t* buffer;
    Lock lock;
} Window;

void compositor_init(void);