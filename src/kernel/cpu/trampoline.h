#pragma once

#include "smp.h"

#define TRAMPOLINE_PHYSICAL_START ((void*)0x8000)
#define TRAMPOLINE_PML_ADDRESS ((void*)0x8FF0)
#define TRAMPOLINE_STACK_TOP_ADDRESS ((void*)0x8FE0)
#define TRAMPOLINE_ENTRY_ADDRESS ((void*)0x8FD0)

extern void trampoline_virtual_start(void);

void trampoline_init(void);

void trampoline_cpu_setup(uint64_t rsp);

void trampoline_deinit(void);
