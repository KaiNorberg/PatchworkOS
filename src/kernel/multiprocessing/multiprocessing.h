#pragma once

#include <stdint.h>

void multiprocessing_init(void* entry);

uint8_t multiprocessing_get_cpu_amount();