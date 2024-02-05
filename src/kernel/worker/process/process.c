#include "process.h"

#include "heap/heap.h"
#include "page_allocator/page_allocator.h"
#include "tty/tty.h"
#include "lock/lock.h"
#include "gdt/gdt.h"
#include "debug/debug.h"

#include <stdatomic.h>

atomic_size_t pid;

void pid_init()
{
    atomic_init(&pid, 1);
}

uint64_t pid_new()
{
    return atomic_fetch_add_explicit(&pid, 1, memory_order_seq_cst);
}

Process* process_new()
{
    Process* newProcess = kmalloc(sizeof(Process));

    newProcess->pageDirectory = page_directory_new();
    newProcess->memoryBlocks = vector_new(sizeof(MemoryBlock));
    newProcess->id = pid_new();
    newProcess->taskAmount = 0;

    process_allocate_page(newProcess, (void*)(USER_ADDRESS_SPACE_TOP - 0x1000));
    
    return newProcess;
}

void* process_allocate_page(Process* process, void* virtualAddress)
{
    void* physicalAddress = page_allocator_request();

    MemoryBlock newBlock;
    newBlock.physicalAddress = physicalAddress;
    newBlock.virtualAddress = virtualAddress;

    vector_push_back(process->memoryBlocks, &newBlock);

    page_directory_remap(process->pageDirectory, virtualAddress, physicalAddress, PAGE_DIR_READ_WRITE | PAGE_DIR_USER_SUPERVISOR);

    return physicalAddress;
}

Task* task_new(Process* process, uint8_t priority)
{
    if (priority > TASK_PRIORITY_MAX)
    {
        debug_panic("Priority level out of bounds");
    }

    process->taskAmount++;

    Task* newTask = kmalloc(sizeof(Task));
    newTask->process = process;
    newTask->interruptFrame = interrupt_frame_new(0, (void*)USER_ADDRESS_SPACE_TOP, GDT_USER_CODE | 3, GDT_USER_DATA | 3, process->pageDirectory);
    newTask->state = TASK_STATE_READY;
    newTask->priority = priority;

    return newTask;
}

void task_free(Task* task)
{        
    Process* process = task->process;

    interrupt_frame_free(task->interruptFrame);
    kfree(task);

    process->taskAmount--;
    if (process->taskAmount == 0)
    {
        page_directory_free(process->pageDirectory);

        for (uint64_t i = 0; i < process->memoryBlocks->length; i++)
        {
            MemoryBlock* memoryBlock = vector_get(process->memoryBlocks, i);

            page_allocator_unlock_page(memoryBlock->physicalAddress);
        }
        vector_free(process->memoryBlocks);

        kfree(process);
    }
}