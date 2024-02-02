#include "time.h"

#include "tty/tty.h"
#include "time/time.h"

#include "master/interrupts/interrupts.h"
#include "master/dispatcher/dispatcher.h"

void time_job_init()
{
    dispatcher_push(time_job, IRQ_SLOW_TIMER);
}

void time_job()
{        
    //Temporary for testing
    tty_acquire();
    Point cursorPos = tty_get_cursor_pos();
    tty_set_cursor_pos(0, 16);
    tty_print("MASTER | SLOW: "); 
    tty_printx(time_nanoseconds()); 
    tty_set_cursor_pos(cursorPos.x, cursorPos.y);
    tty_release();

    time_accumulate();

    dispatcher_wait(time_job, IRQ_SLOW_TIMER);
}