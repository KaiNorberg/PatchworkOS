#pragma once

#include "defs.h"
#include "smp.h"

#define TRAMPOLINE_PHYSICAL_START ((void*)0x8000)
#define TRAMPOLINE_PAGE_TABLE_ADDRESS ((void*)0x8FF0)
#define TRAMPOLINE_STACK_TOP_ADDRESS ((void*)0x8FE0)
#define TRAMPOLINE_ENTRY_ADDRESS ((void*)0x8FD0)

extern void trampoline_virtual_start(void);

void trampoline_setup(void);

void trampoline_cpu_setup(Cpu* cpu);

void trampoline_cleanup(void);