#include "schedule.h"

#include "tty/tty.h"
#include "worker_pool/worker_pool.h"

#include "master/interrupts/interrupts.h"
#include "master/dispatcher/dispatcher.h"

void schedule_job_init()
{
    dispatcher_push(schedule_job, IRQ_FAST_TIMER);
}

void schedule_job()
{        
    //Temporary for testing
    tty_acquire();
    Point cursorPos = tty_get_cursor_pos();
    tty_set_cursor_pos(0, 0);
    tty_print("MASTER | FAST: "); 
    tty_printx(time_nanoseconds()); 
    tty_set_cursor_pos(cursorPos.x, cursorPos.y);
    tty_release();

    for (uint16_t i = 0; i < worker_amount(); i++)
    {
        Worker* worker = worker_get((uint8_t)i);

        scheduler_acquire(worker->scheduler);
        scheduler_unblock(worker->scheduler);
        uint8_t wantsToSchedule = scheduler_wants_to_schedule(worker->scheduler);
        scheduler_release(worker->scheduler);

        if (wantsToSchedule)
        {
            Ipi ipi = 
            {
                .type = IPI_WORKER_SCHEDULE
            };
            worker_send_ipi(worker, ipi);
        }
    }
    
    dispatcher_wait(schedule_job, IRQ_FAST_TIMER);
}