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
    
    mainTask = kmalloc(sizeof(Task));
    memset(mainTask, 0, sizeof(Task));

    runningTask = mainTask;

    firstTask = mainTask;
    lastTask = mainTask;
    mainTask->Next = mainTask;
    mainTask->Prev = mainTask;

    tty_end_message(TTY_MESSAGE_OK);
}

Task* multitasking_new(void* entry)
{ 
    Task* newTask = kmalloc(sizeof(Task));
    memset(newTask, 0, sizeof(Task));

    newTask->FirstMemoryBlock = 0;
    newTask->LastMemoryBlock = 0;

    newTask->Context = context_new(entry, 0x18 | 3, 0x20 | 3, 0x202);

    newTask->State = TASK_STATE_READY;

    //Add to linked list
    lastTask->Next = newTask;
    newTask->Prev = lastTask;
    lastTask = newTask;
    newTask->Next = firstTask;
    firstTask->Prev = newTask;
    
    return newTask;
}

void multitasking_free(Task* task)
{
    context_free(task->Context);

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

    kfree(task);
}

void multitasking_schedule()
{
    Task* prev = runningTask;
    prev->State = TASK_STATE_READY;
    
    Task* nextTask = runningTask;
    while (1)
    {
        nextTask = nextTask->Next;

        if (nextTask->State == TASK_STATE_READY)
        {
            break;
        }
        else if (nextTask->Next == prev)
        {
            nextTask = runningTask;
            break;
        }
    }

    runningTask = nextTask;  
    nextTask->State = TASK_STATE_RUNNING;   
}

Task* multitasking_get_running_task()
{
    if (runningTask == mainTask)
    {        
        debug_panic("Failed to retrieve scheduled task!");
    }
    else
    {
        return runningTask;
    }
}

void multitasking_yield_to_user_space()
{
    multitasking_schedule();
    Task* newTask = multitasking_get_running_task();
    mainTask->State = TASK_STATE_WAITING;
    
    jump_to_user_space((void*)newTask->Context->State.InstructionPointer, (void*)newTask->Context->StackTop, (void*)newTask->Context->State.CR3);
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

    virtual_memory_remap((VirtualAddressSpace*)task->Context->State.CR3, physicalAddress, physicalAddress, 1);

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
    
    virtual_memory_remap_pages((VirtualAddressSpace*)task->Context->State.CR3, virtualAddress, physicalAddress, pageAmount, 1);

    return physicalAddress;
}