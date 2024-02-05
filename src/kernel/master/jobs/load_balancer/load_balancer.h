#pragma once

#include <stdint.h>

#define LOAD_BALANCER_ITERATIONS 2

void load_balancer_init();

void load_balancer_iteration(uint64_t average, uint64_t remainder, uint8_t priority);

void load_balancer();