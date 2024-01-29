#pragma once

#include <stdint.h>

#include "idt/idt.h"

#include "worker/worker.h"

void workers_init();

Idt* worker_idt_get();

Worker* worker_get(uint8_t id);

Worker* worker_self();

Worker* worker_self_brute();