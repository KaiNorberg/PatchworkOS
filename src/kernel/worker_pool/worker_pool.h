#pragma once

#include <stdint.h>

#include "idt/idt.h"

#include "worker/worker.h"

void worker_pool_init();

void worker_pool_send_ipi(Ipi ipi);

void worker_pool_spawn(const char* path);

uint8_t worker_amount();

Idt* worker_idt_get();

Worker* worker_get(uint8_t id);

Worker* worker_self();

Worker* worker_self_brute();