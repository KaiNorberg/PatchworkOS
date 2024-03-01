#pragma once

#include <stdint.h>

#include "cpu/cpu.h"
#include "ipi/ipi.h"

#define MAX_CPU_AMOUNT 256

void smp_init();

void smp_send_ipi(Cpu* cpu, Ipi ipi);

Ipi smp_receive_ipi();

uint8_t smp_cpu_amount();

Cpu* smp_cpu(uint8_t id);

Cpu* smp_self();

Cpu* smp_self_brute();