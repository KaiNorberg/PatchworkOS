#pragma once

#include "workers/worker/worker.h"

#define WORKER_TRAMPOLINE_LOADED_START ((void*)0x8000)
#define WORKER_TRAMPOLINE_PAGE_DIRECTORY_ADDRESS 0x8FF0
#define WORKER_TRAMPOLINE_STACK_TOP_ADDRESS 0x8FE0
#define WORKER_TRAMPOLINE_ENTRY_ADDRESS 0x8FD0

#define WORKER_TRAMPOLINE_SIZE ((uint64_t)worker_trampoline_end - (uint64_t)worker_trampoline_start)

extern void worker_trampoline_start();
extern void worker_trampoline_end();

void workers_startup(Worker workers[], uint8_t* workerAmount);