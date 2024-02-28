#pragma once

#include <stdint.h>

extern uint64_t _programLoaderStart;
extern uint64_t _programLoaderEnd;

void program_loader_init();

void program_loader_entry(const char* executable) __attribute__((section (".program_loader")));
