#pragma once

#include "defs.h"
#include "lock.h"
#include "list.h"

#include <common/boot_info.h>

#include <sys/win.h>

typedef struct
{
    ListEntry base;
    win_info_t info;
    pixel_t* buffer;
    Lock lock;
} Window;

void compositor_init(GopBuffer* gopBuffer);