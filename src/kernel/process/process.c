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
    process->addressSpace = address_space_new();
    process->fileTable = file_table_new();
    process->kernelStackBottom = kmalloc(PAGE_SIZE);
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
    address_space_free(process->addressSpace);
    file_table_free(process->fileTable);
    kfree(process->kernelStackBottom);
    kfree(process);
}