#pragma once

#include <stdint.h>

#include "kernel/virtual_memory/virtual_memory.h"

void syscall_init(VirtualAddressSpace* addressSpace, uint64_t* stack);