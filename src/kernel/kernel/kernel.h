#pragma once

#include <common/boot_info/boot_info.h>

#include "types/types.h"

void kernel_init(BootInfo* bootInfo);

void kernel_cpu_init();