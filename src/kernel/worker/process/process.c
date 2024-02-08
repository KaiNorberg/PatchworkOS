#include "process.h"

#include "heap/heap.h"
#include "page_allocator/page_allocator.h"
#include "tty/tty.h"
#include "lock/lock.h"
#include "gdt/gdt.h"
#include "debug/debug.h"
#include "utils/utils.h"

#include <stdatomic.h>

#include <libc/string.h>

atomic_size_t pid;

void pid_init()
{
    atomic_init(&pid, 1);
}

uint64_t pid_new()
{
    return atomic_fetch_add_explicit(&pid, 1, memory_order_seq_cst);
}

Process* process_new(uint8_t priority)
{
    if (priority > PROCESS_PRIORITY_MAX)
    {
        debug_panic("Priority level out of bounds");
    }

    Process* process = kmalloc(sizeof(Process));
    memset(process, 0, sizeof(Process));

    process->id = pid_new();
    process->pageDirectory = page_directory_new();
    process->memoryBlocks = vector_new(sizeof(MemoryBlock));
    process->fileTable = file_table_new();
    process->interruptFrame = interrupt_frame_new(0, (void*)USER_ADDRESS_SPACE_TOP, GDT_USER_CODE | 3, GDT_USER_DATA | 3, process->pageDirectory);
    process->status = STATUS_SUCCESS;
    process->state = PROCESS_STATE_READY;
    process->priority = priority;

    process_allocate_pages(process, (void*)(USER_ADDRESS_SPACE_TOP - 0x1000), 1);
    
    return process;
}

void* process_allocate_pages(Process* process, void* virtualAddress, uint64_t amount)
{
    void* physicalAddress = page_allocator_request_amount(amount);

    MemoryBlock newBlock;
    newBlock.physicalAddress = physicalAddress;
    newBlock.virtualAddress = virtualAddress;
    newBlock.pageAmount = amount;

    vector_push_back(process->memoryBlocks, &newBlock);

    page_directory_remap_pages(process->pageDirectory, virtualAddress, physicalAddress, amount, PAGE_DIR_READ_WRITE | PAGE_DIR_USER_SUPERVISOR);

    return physicalAddress;
}

void process_free(Process* process)
{
    page_directory_free(process->pageDirectory);
    for (uint64_t i = 0; i < process->memoryBlocks->length; i++)
    {
        MemoryBlock* memoryBlock = vector_get(process->memoryBlocks, i);

        page_allocator_unlock_pages(memoryBlock->physicalAddress, memoryBlock->pageAmount);
    }
    vector_free(process->memoryBlocks);

    file_table_free(process->fileTable);
    interrupt_frame_free(process->interruptFrame);

    kfree(process);
}