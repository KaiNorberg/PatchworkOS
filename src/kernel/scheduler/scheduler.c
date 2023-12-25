#include "scheduler.h"

#include "tty/tty.h"
#include "heap/heap.h"
#include "debug/debug.h"
#include "string/string.h"
#include "io/io.h"
#include "queue/queue.h"

Process* runningProcess;

Queue* readyProcessQueue;

void scheduler_visualize()
{
    /*Pixel black;
    black.a = 255;
    black.r = 0;
    black.g = 0;
    black.b = 0;

    Pixel green;
    green.a = 255;
    green.r = 152;
    green.g = 195;
    green.b = 121;

    Pixel red;
    red.a = 255;
    red.r = 224;
    red.g = 108;
    red.b = 117;

    Pixel blue;
    blue.a = 255;
    blue.r = 97;
    blue.g = 175;
    blue.b = 239;

    tty_print("Scheduler visualization (blue = running, green = ready, red = waiting):\n\r");
    int i = 0;
    Process* currentProcess = firstProcess;
    while (currentProcess != 0)
    {
        if (currentProcess->state == PROCESS_STATE_RUNNING)
        {
            tty_set_background(blue);
        }
        else if (currentProcess->state == PROCESS_STATE_READY)
        {
            tty_set_background(green);
        }
        else
        {
            tty_set_background(red);
        }

        tty_put(' '); tty_printi(i); tty_put(' ');
    
        i++;
        currentProcess = currentProcess->next;

        if (currentProcess == firstProcess)
        {
            break;
        }
    }

    tty_set_background(black);
    tty_print("\n\n\r");*/
}

void scheduler_init()
{
    tty_start_message("Scheduler initializing");
    
    readyProcessQueue = queue_new();

    runningProcess = 0;

    tty_end_message(TTY_MESSAGE_OK);
}

void scheduler_sleep(Process* process)
{
    
}

void scheduler_append(Process* process)
{
    queue_push(readyProcessQueue, process);
}

void scheduler_remove(Process* process)
{
    /*if (process == runningProcess)
    {
        scheduler_schedule();
    }

    if (process == firstProcess)
    {
        firstProcess = process->next;
    }
    if (process == lastProcess)
    {
        lastProcess = process->prev;
    }
    process->next->prev = process->prev;
    process->prev->next = process->next;*/
}

void scheduler_schedule()
{
    Process* nextProcess = queue_pop(readyProcessQueue);    
    if (nextProcess != 0)
    {
        runningProcess->state = PROCESS_STATE_READY;
        queue_push(readyProcessQueue, runningProcess);

        runningProcess = nextProcess;
        runningProcess->state = PROCESS_STATE_RUNNING;       
    }
}

Process* scheduler_get_running_process()
{
    if (runningProcess == 0)
    {
        debug_panic("Failed to retrieve scheduled process!");
        return 0;
    }
    else
    {
        return runningProcess;
    }
}

void scheduler_yield_to_user_space()
{
    runningProcess = queue_pop(readyProcessQueue);
    runningProcess->state = PROCESS_STATE_RUNNING; 
    
    io_pic_clear_mask(IRQ_PIT);

    jump_to_user_space((void*)runningProcess->context->state.instructionPointer, (void*)runningProcess->context->state.stackPointer, (void*)runningProcess->context->state.cr3);
}