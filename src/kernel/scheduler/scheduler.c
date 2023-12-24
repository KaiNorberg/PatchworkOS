#include "scheduler.h"

#include "tty/tty.h"
#include "heap/heap.h"
#include "debug/debug.h"
#include "string/string.h"
#include "io/io.h"

Process* mainProcess;

Process* runningProcess;

Process* firstProcess;
Process* lastProcess;

void scheduler_visualize()
{
    Pixel black;
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
        else if (currentProcess->state == PROCESS_STATE_WAITING)
        {
            tty_set_background(red);
        }
        else if (currentProcess->state == PROCESS_STATE_READY)
        {
            tty_set_background(green);
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
    tty_print("\n\n\r");
}

void scheduler_init()
{
    tty_start_message("Scheduler initializing");
    
    mainProcess = kmalloc(sizeof(Process));
    mainProcess->context = context_new(0, 0, 0, 0, 0, 0);
    memset(mainProcess, 0, sizeof(Process));

    runningProcess = mainProcess;

    firstProcess = mainProcess;
    lastProcess = mainProcess;
    mainProcess->next = mainProcess;
    mainProcess->prev = mainProcess;

    tty_end_message(TTY_MESSAGE_OK);
}

void scheduler_append(Process* process)
{
    lastProcess->next = process;
    process->prev = lastProcess;
    lastProcess = process;
    process->next = firstProcess;
    firstProcess->prev = process;
}

void scheduler_remove(Process* process)
{
    if (process == runningProcess)
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
    process->prev->next = process->next;
}

void scheduler_schedule()
{
    Process* prev = runningProcess;
    prev->state = PROCESS_STATE_READY;
    
    Process* nextProcess = runningProcess;
    while (1)
    {
        nextProcess = nextProcess->next;

        if (nextProcess->state == PROCESS_STATE_READY)
        {
            break;
        }
        else if (nextProcess->next == prev)
        {
            nextProcess = runningProcess;
            break;
        }
    }

    runningProcess = nextProcess;  
    nextProcess->state = PROCESS_STATE_RUNNING;   
}

Process* scheduler_get_running_process()
{
    if (runningProcess == mainProcess)
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
    scheduler_schedule();
    Process* newProcess = scheduler_get_running_process();
    mainProcess->state = PROCESS_STATE_WAITING; 
    
    io_pic_clear_mask(IRQ_PIT);

    jump_to_user_space((void*)newProcess->context->state.instructionPointer, (void*)newProcess->context->state.stackPointer, (void*)newProcess->context->state.cr3);
}