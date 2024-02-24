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
    page_directory_map_pages(pageDirectory, WORKER_TRAMPOLINE_PHYSICAL_START, WORKER_TRAMPOLINE_PHYSICAL_START, 1, PAGE_FLAG_WRITE);

    backupBuffer = kmalloc(0x1000);
    memcpy(backupBuffer, vmm_physical_to_virtual(WORKER_TRAMPOLINE_PHYSICAL_START), WORKER_TRAMPOLINE_SIZE);

    memcpy(vmm_physical_to_virtual(WORKER_TRAMPOLINE_PHYSICAL_START), worker_trampoline_start, WORKER_TRAMPOLINE_SIZE);

    WRITE_64(vmm_physical_to_virtual(WORKER_TRAMPOLINE_PAGE_DIRECTORY_ADDRESS), (uint64_t)pageDirectory);
    WRITE_64(vmm_physical_to_virtual(WORKER_TRAMPOLINE_ENTRY_ADDRESS), worker_entry);
}

void worker_trampoline_worker_setup(Worker* worker)
{
    WRITE_64(vmm_physical_to_virtual(WORKER_TRAMPOLINE_STACK_TOP_ADDRESS), (void*)worker->tss->rsp0);
}

void worker_trampoline_cleanup()
{   
    page_directory_free(pageDirectory); 

    memcpy(vmm_physical_to_virtual(WORKER_TRAMPOLINE_PHYSICAL_START), backupBuffer, WORKER_TRAMPOLINE_SIZE);
    kfree(backupBuffer);
}