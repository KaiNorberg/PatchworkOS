#pragma once

#include "ipi/ipi.h"
#include "tss/tss.h"
#include "scheduler/scheduler.h"

#include <stdint.h>

#define MAX_WORKER_AMOUNT 255

#define IPI_WORKER_NONE 0
#define IPI_WORKER_HALT 1
#define IPI_WORKER_SCHEDULE 2

typedef struct
{
    uint8_t present;
    uint8_t running;
    
    uint8_t id; 
    uint8_t apicId;

    Ipi ipi;

    Tss* tss;
    Scheduler* scheduler;
} Worker;

uint8_t worker_init(Worker* worker, uint8_t id, uint8_t apicId);

void worker_entry();

Ipi worker_receive_ipi();

void worker_send_ipi(Worker* worker, Ipi ipi);