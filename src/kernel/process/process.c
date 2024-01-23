#include "process.h"

#include "heap/heap.h"
#include "string/string.h"
#include "page_allocator/page_allocator.h"
#include "tty/tty.h"
#include "spin_lock/spin_lock.h"

#include <stdatomic.h>

atomic_ullong pid;

void pid_init()
{
    pid = 1;
}

uint64_t pid_new()
{
    return __atomic_fetch_add(&pid, 1, __ATOMIC_SEQ_CST);
}

Process* process_new()
{
    Process* newProcess = kmalloc(sizeof(Process));

    newProcess->pageDirectory = page_directory_new();

    newProcess->firstMemoryBlock = 0;
    newProcess->lastMemoryBlock = 0;

    newProcess->id = pid_new();

    process_allocate_pages(newProcess, PROCESS_ADDRESS_SPACE_USER_STACK, 1);
    
    return newProcess;
}

void process_free(Process* process)
{
    page_directory_free(process->pageDirectory);

    if (process->firstMemoryBlock != 0)
    {
        MemoryBlock* currentBlock = process->firstMemoryBlock;
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

    kfree(process);
}

void* process_allocate_pages(Process* process, void* virtualAddress, uint64_t pageAmount)
{
    MemoryBlock* newMemoryBlock = kmalloc(sizeof(MemoryBlock));

    void* physicalAddress = page_allocator_request_amount(pageAmount);

    newMemoryBlock->physicalAddress = physicalAddress;
    newMemoryBlock->virtualAddress = virtualAddress;
    newMemoryBlock->pageAmount = pageAmount;
    newMemoryBlock->next = 0;

    if (process->firstMemoryBlock == 0)
    {
        process->firstMemoryBlock = newMemoryBlock;
        process->lastMemoryBlock = newMemoryBlock;
    }
    else
    {
        process->lastMemoryBlock->next = newMemoryBlock;
        process->lastMemoryBlock = newMemoryBlock;
    }
    
    page_directory_remap_pages(process->pageDirectory, virtualAddress, physicalAddress, pageAmount, PAGE_DIR_READ_WRITE | PAGE_DIR_USER_SUPERVISOR);

    return physicalAddress;
}