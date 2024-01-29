#pragma once

#include "idt/idt.h"

#include <stdint.h>

typedef struct
{
    uint8_t apicId;
    Idt idt;
} Master;

void master_init();

void master_entry();

uint8_t is_master();

Master* master_get();