#pragma once

#include <bootloader/boot_info.h>

#include "defs.h"

void kernel_init(boot_info_t* bootInfo);

// Will be called on all cpus except the bootstrap cpu during smp_others_init().
void kernel_other_init(void);