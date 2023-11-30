#include "multitasking.h"

#include "common.h"

#include "kernel/kernel/kernel.h"
#include "kernel/heap/heap.h"
#include "kernel/idt/idt.h"
#include "kernel/tty/tty.h"
#include "kernel/page_allocator/page_allocator.h"

Task* mainTask;

Task* runningTask;

Task* firstTask;
Task* lastTask;

void multitasking_visualize()
{    
    Pixel black;
    black.A = 255;
    black.R = 0;
    black.G = 0;
    black.B = 0;

    Pixel green;
    green.A = 255;
    green.R = 152;
    green.G = 195;
    green.B = 121;

    Pixel red;
    red.A = 255;
    red.R = 224;
    red.G = 108;
    red.B = 117;

    Pixel blue;
    blue.A = 255;
    blue.R = 97;
    blue.G = 175;
    blue.B = 239;

    tty_print("\n\rTask visualization (blue = running, green = ready, red = waiting):\n\r");
    int i = 0;
    Task* currentTask = firstTask;
    while (currentTask != 0)
    {
        if (currentTask->State == TASK_STATE_RUNNING)
        {
            tty_set_background(blue);
        }
        else if (currentTask->State == TASK_STATE_WAITING)
        {
            tty_set_background(red);
        }
        else if (currentTask->State == TASK_STATE_READY)
        {
            tty_set_background(green);
        }
        
        tty_put(' '); tty_printi(i); tty_put(' ');
    
        i++;
        currentTask = currentTask->Next;

        if (currentTask == firstTask)
        {
            break;
        }
    }

    tty_set_background(black);
    tty_print("\n\n\r");
}

void multitasking_init()
{
    tty_start_message("Multitasking initializing");
    
    firstTask = 0;
    lastTask = 0;

    mainTask = kmalloc(sizeof(Task));

    asm volatile("mov %%cr3, %%rax; mov %%rax, %0;":"=m"(mainTask->Registers.CR3)::"%rax");

    mainTask->Next = 0;
    mainTask->State = TASK_STATE_RUNNING;

    runningTask = mainTask;

    append_task(mainTask);

    tty_end_message(TTY_MESSAGE_OK);
}

void create_task(void (*main)(), VirtualAddressSpace* addressSpace)
{ 
    Task* newTask = kmalloc(sizeof(Task));

    newTask->Registers.RAX = 0;
    newTask->Registers.RBX = 0;
    newTask->Registers.RCX = 0;
    newTask->Registers.RDX = 0;
    newTask->Registers.Rflags = 0;
    newTask->Registers.RIP = (uint64_t)main;
    newTask->Registers.CR3 = (uint64_t)addressSpace;
    newTask->Registers.RSP = (uint64_t)page_allocator_request() + 4096;
    newTask->Next = 0;
    newTask->State = TASK_STATE_READY;

    append_task(newTask);
}

void append_task(Task* task)
{
    if (firstTask == 0)
    {
        firstTask = task;
        lastTask = task;
        firstTask->Next = task;
        lastTask->Next = task;
    }
    else
    {            
        lastTask->Next = task;

        lastTask = task;
        task->Next = firstTask;
    }
}

void return_to_kernel()
{
    mainTask->State = TASK_STATE_READY;
    append_task(mainTask);
}

void yield() 
{    
    Task* prev = runningTask;
    prev->State = TASK_STATE_WAITING;
    
    while (1)
    {
        runningTask = runningTask->Next;

        if (runningTask->State != TASK_STATE_WAITING)
        {
            break;
        }
        else if (runningTask->Next == prev)
        {
            runningTask = mainTask;
            break;
        }
    }

    runningTask->State = TASK_STATE_RUNNING;
    switch_registers(&prev->Registers, &runningTask->Registers);
}

void exit(uint64_t status)
{
    //Temporary


    yield();
}