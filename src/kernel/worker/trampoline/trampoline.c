#include "trampoline.h"

#include <libc/string.h>

#include "page_directory/page_directory.h"
#include "utils/utils.h"
#include "heap/heap.h"
#include "vmm/vmm.h"
#include "worker/worker.h"

static PageDirectory* pageDirectory;
static void* backupBuffer;

void worker_trampoline_setup()
{
    pageDirectory = page_directory_new();
    vmm_map_kernel(pageDirectory);
    page_directory_map(pageDirectory, WORKER_TRAMPOLINE_PHYSICAL_START, WORKER_TRAMPOLINE_PHYSICAL_START, PAGE_FLAG_WRITE);

    backupBuffer = kmalloc(PAGE_SIZE);
    memcpy(backupBuffer, vmm_physical_to_virtual(WORKER_TRAMPOLINE_PHYSICAL_START), PAGE_SIZE);

    memcpy(vmm_physical_to_virtual(WORKER_TRAMPOLINE_PHYSICAL_START), worker_trampoline_virtual_start, PAGE_SIZE);

    WRITE_64(vmm_physical_to_virtual(WORKER_TRAMPOLINE_PAGE_DIRECTORY_ADDRESS), (uint64_t)pageDirectory);
    WRITE_64(vmm_physical_to_virtual(WORKER_TRAMPOLINE_ENTRY_ADDRESS), worker_entry);
}

void worker_trampoline_specific_setup(Worker* worker)
{
    WRITE_64(vmm_physical_to_virtual(WORKER_TRAMPOLINE_STACK_TOP_ADDRESS), (void*)worker->tss->rsp0);
}

void worker_trampoline_cleanup()
{   
    memcpy(vmm_physical_to_virtual(WORKER_TRAMPOLINE_PHYSICAL_START), backupBuffer, PAGE_SIZE);
    kfree(backupBuffer);

    page_directory_free(pageDirectory);
}