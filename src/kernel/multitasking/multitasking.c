#include "multitasking.h"

#include "common.h"

#include "kernel/kernel/kernel.h"
#include "kernel/heap/heap.h"
#include "kernel/idt/idt.h"
#include "kernel/tty/tty.h"
#include "kernel/page_allocator/page_allocator.h"
#include "kernel/string/string.h"

Task* dummyTask;

Task* mainTask;

Task* runningTask;

Task* firstTask;
Task* lastTask;

#define PUSH_REGISTER(RSP, register) uint64_t register; asm volatile("movq %%"#register", %0" : "=r" (register)); push_value_to_stack(RSP, register)

void push_value_to_stack(uint64_t* RSP, uint64_t value)
{
    *RSP -= 8; 
    *((uint64_t*)*RSP) = value;
}

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

    tty_print("Task visualization (blue = running, green = ready, red = waiting):\n\r");
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

void multitasking_init(VirtualAddressSpace* kernelAddressSpace)
{
    tty_start_message("Multitasking initializing");
    
    firstTask = 0;
    lastTask = 0;

    dummyTask = kmalloc(sizeof(Task));
    dummyTask->StackPointer = (uint64_t)page_allocator_request() + 0x1000;

    mainTask = kmalloc(sizeof(Task));
    mainTask->AddressSpace = kernelAddressSpace;
    mainTask->State = TASK_STATE_RUNNING;
    mainTask->Next = 0;
    mainTask->Prev = 0;

    runningTask = mainTask;

    append_task(mainTask);

    tty_end_message(TTY_MESSAGE_OK);
}

Task* load_next_task()
{
    Task* prev = runningTask;
    prev->State = TASK_STATE_WAITING;       

    Task* nextTask = get_next_ready_task(runningTask);
    runningTask = nextTask;  

    nextTask->State = TASK_STATE_RUNNING;     

    return nextTask;
}

Task* get_running_task()
{
    return runningTask;
}

Task* get_next_ready_task(Task* task)
{
    Task* prev = task;
        
    while (1)
    {
        task = task->Next;

        if (task->State == TASK_STATE_READY)
        {
            return task;
        }
        else if (task->Next == prev)
        {
            return mainTask;
        }
    }
}

void create_task(void (*entry)(), VirtualAddressSpace* addressSpace, void* stackBottom, uint64_t stackSize)
{ 
    Task* newTask = kmalloc(sizeof(Task));
    memset(newTask, 0, sizeof(Task));

    newTask->StackTop = (uint64_t)stackBottom + stackSize;
    newTask->StackBottom = (uint64_t)stackBottom;
    memset((void*)stackBottom, 0, stackSize);

    newTask->StackPointer = newTask->StackTop;
    newTask->AddressSpace = addressSpace;
    newTask->InstructionPointer = (uint64_t)entry;

    newTask->Next = 0;
    newTask->Prev = 0;
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
        firstTask->Prev = task;

        lastTask->Next = task;
        lastTask->Prev = task;
    }
    else
    {            
        lastTask->Next = task;
        firstTask->Prev = task;

        task->Prev = lastTask;
        task->Next = firstTask;
        lastTask = task;
    }
}
/*
void yield() 
{    
    Task* prev = runningTask;
    prev->State = TASK_STATE_WAITING;       

    Task* nextTask = get_next_ready_task(runningTask);
    runningTask = nextTask;

    nextTask->State = TASK_STATE_RUNNING;    
    switch_task(prev, nextTask);
}

void exit(uint64_t status)
{    
    Task* prevTask = lastTask;
    Task* currentTask = firstTask;
    while (1)
    {
        if (currentTask == runningTask)
        {           
            Task* prevRunningTask = runningTask;
            prevRunningTask->State = TASK_STATE_WAITING;   

            Task* nextTask = get_next_ready_task(runningTask);
            prevTask->Next = runningTask->Next;

            page_allocator_unlock_page((void*)(currentTask->StackTop - 0x1000));
            kfree(currentTask);

            runningTask = nextTask;
            nextTask->State = TASK_STATE_RUNNING;    
            switch_task(dummyTask, nextTask);
        }
        
        prevTask = currentTask;
        currentTask = currentTask->Next;
    
        if (currentTask == firstTask)
        {
            tty_print("ERROR: Unable to find task!");
            break;
        }
    }
}
*/