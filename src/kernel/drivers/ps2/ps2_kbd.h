#pragma once

#include "ps2.h"

typedef struct
{
    bool isExtended;
} ps2_kbd_irq_context_t;

uint64_t ps2_kbd_init(ps2_device_info_t* info);
