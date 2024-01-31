#pragma once

#include "idt/idt.h"

#include <stdint.h>

#define MASTER_TIMER_HZ 1024

void master_init();

void master_entry();

uint8_t master_apic_id();

uint8_t is_master();