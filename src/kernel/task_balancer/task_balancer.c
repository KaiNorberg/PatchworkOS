#include "task_balancer.h"

#include "kernel_process/kernel_process.h"
#include "scheduler/scheduler.h"
#include "tty/tty.h"
#include "time/time.h"

void task_balancer_init()
{
    tty_start_message("Task Balancer Initilizing");

    Task* taskBalancer = kernel_task_new(task_balancer_entry);
    scheduler_push(taskBalancer);

    tty_end_message(TTY_MESSAGE_OK);
}

void task_balancer_entry()
{
    while (1)
    {
        scheduler_acquire_all();
        
        for (uint8_t priority = TASK_PRIORITY_MIN; priority <= TASK_PRIORITY_MAX; priority++)
        {
            uint64_t totalTasks = 0;
            for (int i = 0; i < smp_cpu_amount(); i++)
            {
                totalTasks += queue_length(scheduler_get(i)->queues[priority]) + (scheduler_get(i)->runningTask != 0);
            }

            uint64_t averageLoad = totalTasks / smp_cpu_amount();

            for (int j = 0; j < SCHEDULER_BALANCING_ITERATIONS; j++)
            {
                Task* poppedTask = 0;
                for (int i = 0; i < smp_cpu_amount(); i++)
                {
                    Queue* queue = scheduler_get(i)->queues[priority];
                    uint64_t queueLength = queue_length(queue);

                    uint64_t load = queueLength + (scheduler_get(i)->runningTask != 0);

                    if (queueLength != 0 && load > averageLoad && poppedTask == 0)
                    {
                        poppedTask = queue_pop(queue);
                    }
                    else if (load < averageLoad && poppedTask != 0)
                    {
                        queue_push(queue, poppedTask);
                        poppedTask = 0;
                    }
                }
            
                if (poppedTask != 0)
                {
                    queue_push(scheduler_get(0)->queues[priority], poppedTask);
                }
            }
        }

        scheduler_release_all();

        kernel_task_block(time_nanoseconds() + NANOSECONDS_PER_SECOND);
    }
}