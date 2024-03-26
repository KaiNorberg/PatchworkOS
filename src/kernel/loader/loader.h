#pragma once

#include "defs/defs.h"

//All of this is temporary for testing because we dont have a vfs to load programs

extern uint64_t _loaderStart;
extern uint64_t _loaderEnd;

extern void loader_entry();

void loader_init();

void* loader_allocate_stack();

void* loader_load() __attribute__((section(".loader")));
