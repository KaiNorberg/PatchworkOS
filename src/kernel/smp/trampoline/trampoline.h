#pragma once

#include "types/types.h"
#include "smp/smp.h"

#define SMP_TRAMPOLINE_PHYSICAL_START ((void*)0x8000)
#define SMP_TRAMPOLINE_PAGE_DIRECTORY_ADDRESS ((void*)0x8FF0)
#define SMP_TRAMPOLINE_STACK_TOP_ADDRESS ((void*)0x8FE0)
#define SMP_TRAMPOLINE_ENTRY_ADDRESS ((void*)0x8FD0)

extern void smp_trampoline_virtual_start(void);

void smp_trampoline_setup(void);

void smp_trampoline_cpu_setup(Cpu* cpu);

void smp_trampoline_cleanup(void);