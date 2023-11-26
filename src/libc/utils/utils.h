#pragma once

#include <stdint.h>

#include "common.h"

uint64_t syscall_helper(uint64_t rax, uint64_t rdi, uint64_t rsi, uint64_t rdx);