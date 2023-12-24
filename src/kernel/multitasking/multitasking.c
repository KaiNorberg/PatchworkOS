#include "multitasking.h"

#include "../common.h"

#include "kernel/kernel.h"
#include "heap/heap.h"
#include "idt/idt.h"
#include "tty/tty.h"
#include "page_allocator/page_allocator.h"
#include "string/string.h"
#include "debug/debug.h"
#include "time/time.h"
#include "io/io.h"

Task* mainTask;

Task* runningTask;

Task* firstTask;
Task* lastTask;

void multitasking_visualize()
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

    tty_print("Task visualization (blue = running, green = ready, red = waiting):\n\r");
    int i = 0;
    Task* currentTask = firstTask;
    while (currentTask != 0)
    {
        if (currentTask->state == TASK_STATE_RUNNING)
        {
            tty_set_background(blue);
        }
        else if (currentTask->state == TASK_STATE_WAITING)
        {
            tty_set_background(red);
        }
        else if (currentTask->state == TASK_STATE_READY)
        {
            tty_set_background(green);
        }
        
        tty_put(' '); tty_printi(i); tty_put(' ');
    
        i++;
        currentTask = currentTask->next;

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
    mainTask->context = context_new(0, 0, 0, 0, 0, 0);
    memset(mainTask, 0, sizeof(Task));

    runningTask = mainTask;

    firstTask = mainTask;
    lastTask = mainTask;
    mainTask->next = mainTask;
    mainTask->prev = mainTask;

    tty_end_message(TTY_MESSAGE_OK);
}

Task* multitasking_new(void* entry)
{ 
    Task* newTask = kmalloc(sizeof(Task));
    memset(newTask, 0, sizeof(Task));

    newTask->firstMemoryBlock = 0;
    newTask->lastMemoryBlock = 0;

    newTask->pageDirectory = page_directory_create();

    task_request_page(newTask, USER_ADDRESS_SPACE_STACK_TOP_PAGE);

    newTask->context = context_new(entry, USER_ADDRESS_SPACE_STACK_TOP_PAGE + 0x1000, 0x18 | 3, 0x20 | 3, 0x202, newTask->pageDirectory);
    newTask->state = TASK_STATE_READY;

    //Add to linked list
    lastTask->next = newTask;
    newTask->prev = lastTask;
    lastTask = newTask;
    newTask->next = firstTask;
    firstTask->prev = newTask;
    
    return newTask;
}

void multitasking_free(Task* task)
{
    if (task == runningTask)
    {
        multitasking_schedule();
    }

    context_free(task->context);
    page_directory_erase(task->pageDirectory);

    if (task->firstMemoryBlock != 0)
    {
        MemoryBlock* currentBlock = task->firstMemoryBlock;
        while (1)
        {
            MemoryBlock* nextBlock = currentBlock->next;

            page_allocator_unlock_pages(currentBlock->physicalAddress, currentBlock->pageAmount);

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
        firstTask = task->next;
    }
    if (task == lastTask)
    {
        lastTask = task->prev;
    }
    task->next->prev = task->prev;
    task->prev->next = task->next;

    kfree(task);
}

void multitasking_schedule()
{
    Task* prev = runningTask;
    prev->state = TASK_STATE_READY;
    
    Task* nextTask = runningTask;
    while (1)
    {
        nextTask = nextTask->next;

        if (nextTask->state == TASK_STATE_READY)
        {
            break;
        }
        else if (nextTask->next == prev)
        {
            nextTask = runningTask;
            break;
        }
    }

    runningTask = nextTask;  
    nextTask->state = TASK_STATE_RUNNING;   
}

Task* multitasking_get_running_task()
{
    if (runningTask == mainTask)
    {
        debug_panic("Failed to retrieve scheduled task!");
        return 0;
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
    mainTask->state = TASK_STATE_WAITING; 
    
    io_pic_clear_mask(IRQ_PIT);

    jump_to_user_space((void*)newTask->context->state.instructionPointer, (void*)newTask->context->state.stackPointer, (void*)newTask->context->state.cr3);
}

void* task_request_page(Task* task, void* virtualAddress)
{
    MemoryBlock* newMemoryBlock = kmalloc(sizeof(MemoryBlock));

    void* physicalAddress = page_allocator_request();

    newMemoryBlock->physicalAddress = physicalAddress;
    newMemoryBlock->virtualAddress = virtualAddress;
    newMemoryBlock->pageAmount = 1;
    newMemoryBlock->next = 0;

    if (task->firstMemoryBlock == 0)
    {
        task->firstMemoryBlock = newMemoryBlock;
        task->lastMemoryBlock = newMemoryBlock;
    }
    else
    {
        task->lastMemoryBlock->next = newMemoryBlock;
        task->lastMemoryBlock = newMemoryBlock;
    }

    page_directory_remap(task->pageDirectory, virtualAddress, physicalAddress, PAGE_DIR_READ_WRITE | PAGE_DIR_USER_SUPERVISOR);

    return physicalAddress;
}

void* task_allocate_pages(Task* task, void* virtualAddress, uint64_t pageAmount)
{
    MemoryBlock* newMemoryBlock = kmalloc(sizeof(MemoryBlock));

    void* physicalAddress = page_allocator_request_amount(pageAmount);

    newMemoryBlock->physicalAddress = physicalAddress;
    newMemoryBlock->virtualAddress = virtualAddress;
    newMemoryBlock->pageAmount = pageAmount;
    newMemoryBlock->next = 0;

    if (task->firstMemoryBlock == 0)
    {
        task->firstMemoryBlock = newMemoryBlock;
        task->lastMemoryBlock = newMemoryBlock;
    }
    else
    {
        task->lastMemoryBlock->next = newMemoryBlock;
        task->lastMemoryBlock = newMemoryBlock;
    }
    
    page_directory_remap_pages(task->pageDirectory, virtualAddress, physicalAddress, pageAmount, PAGE_DIR_READ_WRITE | PAGE_DIR_USER_SUPERVISOR);

    return physicalAddress;
}