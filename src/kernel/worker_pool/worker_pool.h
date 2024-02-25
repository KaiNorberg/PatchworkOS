#pragma once

#include <stdint.h>

#include "idt/idt.h"
#include "worker/worker.h"
#include "ipi/ipi.h"

#define MAX_WORKER_AMOUNT 255

void worker_pool_init();

void worker_pool_send_ipi(Ipi ipi);

//Temporary
void worker_pool_spawn(const char* path);

uint8_t worker_amount();

Worker* worker_get(uint8_t id);

Worker* worker_self();

Worker* worker_self_brute();