#pragma once

#include <stdint.h>

__attribute__((interrupt)) void keyboard_interrupt(void* frame);