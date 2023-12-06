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

uint64_t pageMapPageAmount;

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

    pageMapPageAmount = (page_allocator_get_total_amount() / 8) / 0x1000 + 1;

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

void* task_allocate_memory(Task* task, void* virtualAddress, uint64_t size)
{
    uint64_t pageAmount = size / 0x1000 + 1;
    void* physicalAddress = page_allocator_request_amount(pageAmount);

    for (uint64_t i = 0; i < pageAmount; i++)
    {
        uint64_t index = (uint64_t)(physicalAddress + i * 0x1000) / 0x1000;
        task->PageMap[index] = task->PageMap[index / 64] | (uint64_t)1 << (index % 64);

        virtual_memory_remap(task->AddressSpace, virtualAddress + i * 0x1000, physicalAddress + i * 0x1000);
    }

    return physicalAddress;
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

Task* create_task(void* entry, VirtualAddressSpace* addressSpace)
{ 
    Task* newTask = kmalloc(sizeof(Task));
    memset(newTask, 0, sizeof(Task));

    newTask->StackBottom = (uint64_t)page_allocator_request();
    newTask->StackTop = (uint64_t)newTask->StackBottom + 0x1000;
    memset((void*)newTask->StackBottom, 0, 0x1000);
    virtual_memory_remap(addressSpace, (void*)newTask->StackBottom, (void*)newTask->StackBottom);

    newTask->StackPointer = newTask->StackTop;
    newTask->AddressSpace = addressSpace;
    newTask->InstructionPointer = (uint64_t)entry;

    newTask->PageMap = page_allocator_request_amount(pageMapPageAmount);
    memset(newTask->PageMap, 0, page_allocator_get_total_amount() / 8);

    newTask->Next = 0;
    newTask->Prev = 0;
    newTask->State = TASK_STATE_READY;

    append_task(newTask);

    return newTask;
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

void erase_task(Task* task)
{
    virtual_memory_erase(task->AddressSpace);
    
    for (uint64_t qwordIndex = 0; qwordIndex < page_allocator_get_total_amount() / 64; qwordIndex++)
    {
        if (task->PageMap[qwordIndex] != 0) //If any bit is not zero
        {            
            for (uint64_t bitIndex = 0; bitIndex < 64; bitIndex++)
            {
                if ((task->PageMap[qwordIndex] & ((uint64_t)1 << bitIndex)) != 0) //If bit is set
                {                    
                    void* address = (void*)((qwordIndex * 64 + bitIndex) * 0x1000);
                    page_allocator_unlock_page(address);
                }
            }
        }
    }

    if (task == firstTask)
    {
        firstTask = task->Next;
    }
    if (task == lastTask)
    {
        lastTask = task->Prev;
    }

    task->Next->Prev = task->Prev;
    task->Prev->Next = task->Next;

    page_allocator_unlock_page((void*)task->StackBottom);
    page_allocator_unlock_pages(task->PageMap, pageMapPageAmount);
    kfree(task);
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