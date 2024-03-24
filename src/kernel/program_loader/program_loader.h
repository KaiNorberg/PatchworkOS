#pragma once

#include "types/types.h"

extern uint64_t _programLoaderStart;
extern uint64_t _programLoaderEnd;

extern void program_loader_entry(const char* executable);

void program_loader_init();

void* program_loader_load(const char* executable) __attribute__((section(".program_loader")));
