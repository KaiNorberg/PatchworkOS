#pragma once

#include "idt/idt.h"

#include <stdint.h>

extern void master_entry();

void master_init();

uint8_t master_apic_id();

uint8_t is_master();