#include "load_balancer.h"

#include "worker_pool/worker_pool.h"
#include "master/interrupts/interrupts.h"
#include "master/dispatcher/dispatcher.h"
#include "queue/queue.h"
#include "worker/process/process.h"
#include "worker/scheduler/scheduler.h"
#include "worker/worker.h"

static inline void load_balancer_iteration(uint64_t average, uint64_t remainder, uint8_t priority)
{        
    if (average == 0)
    {
        average = 1;
        remainder = 0;
    }

    Process* process = 0;
    for (uint8_t i = 0; i < worker_amount(); i++)
    {
        Worker* worker = worker_get(i);
        Queue* queue = worker->scheduler->queues[priority];

        uint64_t queueLength = queue_length(queue);

        uint64_t amount = queueLength + (worker->scheduler->runningProcess != 0);

        if (remainder != 0 && amount == average + 1)
        {
            remainder--;
        }
        else if (queueLength != 0 && amount > average && process == 0)
        {
            process = queue_pop(queue);
        }
        else if (amount < average && process != 0)
        {
            queue_push(queue, process);
            process = 0;
        }
    }

    if (process != 0)
    {
        queue_push(worker_get(0)->scheduler->queues[priority], process);
    }
}

void load_balancer_init()
{
    dispatcher_push(load_balancer, IRQ_SLOW_TIMER);
}

void load_balancer()
{     
    for (uint8_t i = 0; i < worker_amount(); i++)
    {
        scheduler_acquire(worker_get(i)->scheduler);
    }

    for (uint8_t priority = PROCESS_PRIORITY_MIN; priority <= PROCESS_PRIORITY_MAX; priority++)
    {
        uint64_t total = 0;
        for (uint8_t i = 0; i < worker_amount(); i++)
        {
            Scheduler const* scheduler = worker_get(i)->scheduler;
            total += queue_length(scheduler->queues[priority]) + (scheduler->runningProcess != 0);
        }
        uint64_t average = total / worker_amount();
        uint64_t remainder = total % worker_amount();

        for (int j = 0; j < LOAD_BALANCER_ITERATIONS; j++)
        {
            load_balancer_iteration(average, remainder, priority);
        }
    }

    for (uint8_t i = 0; i < worker_amount(); i++)
    {
        scheduler_release(worker_get(i)->scheduler);
    }

    dispatcher_push(load_balancer, IRQ_SLOW_TIMER);
}