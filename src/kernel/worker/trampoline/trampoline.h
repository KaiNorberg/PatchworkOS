#pragma once

#include <stdint.h>

#include "worker/worker.h"

#define WORKER_TRAMPOLINE_PHYSICAL_START ((void*)0x8000)
#define WORKER_TRAMPOLINE_PAGE_DIRECTORY_ADDRESS ((void*)0x8FF0)
#define WORKER_TRAMPOLINE_STACK_TOP_ADDRESS ((void*)0x8FE0)
#define WORKER_TRAMPOLINE_ENTRY_ADDRESS ((void*)0x8FD0)

#define WORKER_TRAMPOLINE_SIZE ((uint64_t)worker_trampoline_end - (uint64_t)worker_trampoline_start)

extern void worker_trampoline_start();
extern void worker_trampoline_end();

void worker_trampoline_setup();

void worker_trampoline_worker_setup(Worker* worker);

void worker_trampoline_cleanup();