#pragma once

#include <stdint.h>

#define TASK_BALANCER_ITERATIONS 2

void task_balancer_init();

void task_balancer_iteration(uint64_t average, uint64_t remainder, uint8_t priority);

void task_balancer();