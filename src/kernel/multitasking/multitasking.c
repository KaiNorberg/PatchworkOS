#include "multitasking.h"

#include "../common.h"

#include "kernel/kernel.h"
#include "heap/heap.h"
#include "idt/idt.h"
#include "tty/tty.h"
#include "page_allocator/page_allocator.h"
#include "string/string.h"

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

void multitasking_init()
{
    tty_start_message("Multitasking initializing");
    
    firstTask = 0;
    lastTask = 0;

    mainTask = page_allocator_request();
    mainTask->AddressSpace = 0;
    mainTask->State = TASK_STATE_RUNNING;
    mainTask->Next = 0;
    mainTask->Prev = 0;

    runningTask = mainTask;

    multitasking_append(mainTask);

    tty_end_message(TTY_MESSAGE_OK);
}

void multitasking_yield_to_user_space()
{
    //This is a work in progress, dont worry about all the problems

    Task* newTask = load_next_task();
    
    /*memcpy(registerBuffer, &(newTask->Registers), sizeof(RegisterBuffer));
    frame->InstructionPointer = newTask->InstructionPointer;
    frame->StackPointer = newTask->StackPointer;
    taskAddressSpace = (uint64_t)newTask->AddressSpace;*/

    VIRTUAL_MEMORY_LOAD_SPACE(newTask->AddressSpace);

    jump_to_user_space((void*)newTask->InstructionPointer);
}

Task* multitasking_new(void* entry)
{ 
    Task* newTask = kmalloc(sizeof(Task));
    memset(newTask, 0, sizeof(Task));
    
    newTask->FirstMemoryBlock = 0;
    newTask->LastMemoryBlock = 0;

    newTask->StackBottom = (uint64_t)page_allocator_request();
    newTask->StackTop = (uint64_t)newTask->StackBottom + 0x1000;
    memset((void*)newTask->StackBottom, 0, 0x1000);

    newTask->StackPointer = newTask->StackTop;
    newTask->AddressSpace = virtual_memory_create(newTask);
    newTask->InstructionPointer = (uint64_t)entry;

    for (uint64_t i = 0; i < page_allocator_get_total_amount(); i++)
    {
        virtual_memory_remap(newTask->AddressSpace, (void*)(i * 0x1000), (void*)(i * 0x1000));
    }

    //virtual_memory_remap(newTask->AddressSpace, (void*)newTask->StackBottom, (void*)newTask->StackBottom);
    //virtual_memory_remap(newTask->AddressSpace, newTask->AddressSpace, newTask->AddressSpace);

    newTask->Next = 0;
    newTask->Prev = 0;
    newTask->State = TASK_STATE_READY;

    multitasking_append(newTask);

    return newTask;
}

void multitasking_free(Task* task)
{
    virtual_memory_erase(task->AddressSpace);

    if (task->FirstMemoryBlock != 0)
    {
        TaskMemoryBlock* currentBlock = task->FirstMemoryBlock;
        while (1)
        {
            TaskMemoryBlock* nextBlock = task->FirstMemoryBlock->Next;

            page_allocator_unlock_pages(currentBlock->Address, currentBlock->PageAmount);

            kfree(currentBlock);           

            if (nextBlock != 0)
            {
                currentBlock = nextBlock;
            }
            else
            {
                break;
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
    kfree(task);
}

void multitasking_append(Task* task)
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

void* task_request_page(Task* task)
{
    TaskMemoryBlock* newMemoryBlock = kmalloc(sizeof(TaskMemoryBlock));

    void* physicalAddress = page_allocator_request();

    newMemoryBlock->Address = physicalAddress;
    newMemoryBlock->PageAmount = 1;
    newMemoryBlock->Next = 0;

    if (task->FirstMemoryBlock == 0)
    {
        task->FirstMemoryBlock = newMemoryBlock;
        task->LastMemoryBlock = newMemoryBlock;
    }
    else
    {
        task->LastMemoryBlock->Next = newMemoryBlock;
        task->LastMemoryBlock = newMemoryBlock;
    }

    virtual_memory_remap(task->AddressSpace, physicalAddress, physicalAddress);

    return physicalAddress;
}

void* task_allocate_pages(Task* task, void* virtualAddress, uint64_t pageAmount)
{
    TaskMemoryBlock* newMemoryBlock = kmalloc(sizeof(TaskMemoryBlock));

    void* physicalAddress = page_allocator_request_amount(pageAmount);

    newMemoryBlock->Address = physicalAddress;
    newMemoryBlock->PageAmount = pageAmount;
    newMemoryBlock->Next = 0;

    if (task->FirstMemoryBlock == 0)
    {
        task->FirstMemoryBlock = newMemoryBlock;
        task->LastMemoryBlock = newMemoryBlock;
    }
    else
    {
        task->LastMemoryBlock->Next = newMemoryBlock;
        task->LastMemoryBlock = newMemoryBlock;
    }

    for (uint64_t i = 0; i < pageAmount; i++)
    {
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