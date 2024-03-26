#pragma once

#include <common/boot_info/boot_info.h>

#include "defs/defs.h"

void kernel_init(BootInfo* bootInfo);

void kernel_cpu_init();