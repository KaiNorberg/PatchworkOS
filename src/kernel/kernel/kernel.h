#pragma once

#include <stdint.h>
#include <common/boot_info/boot_info.h>

void kernel_init(BootInfo* bootInfo);

void kernel_cpu_init();

void kernel_start();