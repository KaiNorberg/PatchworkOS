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

Process* process_new()
{
    Process* process = kmalloc(sizeof(Process));
    memset(process, 0, sizeof(Process));

    process->pageDirectory = page_directory_new();
    vmm_map_kernel(process->pageDirectory);
    process->fileTable = file_table_new();
    process->lock = lock_new();

    process->id = atomic_fetch_add(&newPid, 1);
    process->refCount = 1;

    return process;
}

Process* process_ref(Process* process)
{
    atomic_fetch_add(&process->refCount, 1);
    return process;
}

void process_unref(Process* process)
{
    if (atomic_fetch_sub(&process->refCount, 1) == 1)
    {
        page_directory_free(process->pageDirectory);
        file_table_free(process->fileTable);
        kfree(process);
    }
}

void* process_allocate_pages(Process* process, void* virtualAddress, uint64_t amount)
{
    void* physicalAddress = pmm_allocate_amount(amount);

    //Page Directory takes ownership of memory.
    lock_acquire(&process->lock);
    page_directory_map_pages(process->pageDirectory, virtualAddress, physicalAddress, amount, PAGE_FLAG_WRITE | PAGE_FLAG_USER_SUPERVISOR);
    lock_release(&process->lock);

    return vmm_physical_to_virtual(physicalAddress);
}