#pragma once

#include <stdint.h>

#include "worker/worker.h"

void workers_init();

Worker* worker_get(uint8_t id);

Worker* worker_self();

Worker* worker_self_brute();