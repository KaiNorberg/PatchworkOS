#include "process.h"

#include <stdatomic.h>
#include <libc/string.h>

#include "heap/heap.h"
#include "lock/lock.h"
#include "vmm/vmm.h"
#include "pmm/pmm.h"
#include "gdt/gdt.h"
#include "debug/debug.h"

static _Atomic uint64_t newPid = 0;

Process* process_new(void* entry)
{
    Process* process = kmalloc(sizeof(Process));
    memset(process, 0, sizeof(Process));
    
    process->id = atomic_fetch_add(&newPid, 1);
    process->pageDirectory = page_directory_new();
    vmm_map_kernel(process->pageDirectory);
    process->fileTable = file_table_new();
    process->kernelStackBottom = vmm_allocate(1);
    process->kernelStackTop = (void*)((uint64_t)process->kernelStackBottom + PAGE_SIZE);
    process->timeStart = 0;
    process->timeEnd = 0;
    process->interruptFrame = interrupt_frame_new(entry, (void*)(VMM_LOWER_HALF_MAX));
    process->status = STATUS_SUCCESS;
    process->state = PROCESS_STATE_ACTIVE;
    process->priority = PROCESS_PRIORITY_MIN;

    return process;
}

void process_free(Process* process)
{
    interrupt_frame_free(process->interruptFrame);
    page_directory_free(process->pageDirectory);
    file_table_free(process->fileTable);
    vmm_free(process->kernelStackBottom, 1);
    kfree(process);
}

void* process_allocate_pages(Process* process, void* virtualAddress, uint64_t amount)
{
    void* physicalAddress = pmm_allocate_amount(amount);

    //Page Directory takes ownership of memory.
    page_directory_map_pages(process->pageDirectory, virtualAddress, physicalAddress, amount, PAGE_FLAG_WRITE | PAGE_FLAG_USER_SUPERVISOR);

    return vmm_physical_to_virtual(physicalAddress);
}