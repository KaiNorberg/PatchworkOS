#pragma once

#include <stdint.h>

#define _EXIT_STACK_SIZE 40

void _ExitStackInit(void);

uint64_t _ExitStackPush(void (*func)(void));

void _ExitStackDispatch(void);
