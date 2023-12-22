#pragma once

#include <stdint.h>

char* itoa(uint64_t i, char b[], uint8_t base);

uint64_t stoi(const char* string);

uint64_t round_up(uint64_t number, uint64_t multiple);

uint64_t round_down(uint64_t number, uint64_t multiple);