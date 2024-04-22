#pragma once

#include <common/boot_info.h>

#include "defs.h"

void kernel_init(BootInfo* bootInfo);

void kernel_cpu_init(void);