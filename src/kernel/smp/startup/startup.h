#pragma once

#include "smp/smp.h"

void smp_entry();

void smp_startup(Cpu cpus[], uint8_t* cpuAmount);