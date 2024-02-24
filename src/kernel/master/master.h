#pragma once

#include <stdint.h>

#include "idt/idt.h"

extern void master_entry();

void master_init();

uint8_t master_apic_id();

uint8_t is_master();