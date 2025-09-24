#pragma once

#include "cpu/trap.h"

#include <boot/boot_info.h>
#include <common/defs.h>
#include <sys/list.h>

typedef struct panic_symbol
{
    list_entry_t entry;
    uintptr_t start;
    uintptr_t end;
    char name[MAX_NAME];
} panic_symbol_t;

void panic_symbols_init(const boot_kernel_t* kernel);

NORETURN void panic(const trap_frame_t* trapFrame, const char* format, ...);
