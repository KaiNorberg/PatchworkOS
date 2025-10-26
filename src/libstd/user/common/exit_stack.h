#pragma once

#include <stdint.h>

#define _EXIT_STACK_SIZE 40

void _exit_stack_init(void);

uint64_t _exit_stack_push(void (*func)(void));

void _exit_stack_dispatch(void);
