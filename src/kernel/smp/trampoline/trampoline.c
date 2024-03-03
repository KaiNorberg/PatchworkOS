#include "trampoline.h"

#include <libc/string.h>

#include "page_directory/page_directory.h"
#include "utils/utils.h"
#include "heap/heap.h"
#include "vmm/vmm.h"

static PageDirectory* pageDirectory;
static void* backupBuffer;

void smp_trampoline_setup()
{
    pageDirectory = page_directory_new();
    vmm_map_kernel(pageDirectory);
    page_directory_map(pageDirectory, SMP_TRAMPOLINE_PHYSICAL_START, SMP_TRAMPOLINE_PHYSICAL_START, PAGE_FLAG_WRITE);

    backupBuffer = kmalloc(PAGE_SIZE);
    memcpy(backupBuffer, vmm_physical_to_virtual(SMP_TRAMPOLINE_PHYSICAL_START), PAGE_SIZE);

    memcpy(vmm_physical_to_virtual(SMP_TRAMPOLINE_PHYSICAL_START), smp_trampoline_virtual_start, PAGE_SIZE);

    WRITE_64(vmm_physical_to_virtual(SMP_TRAMPOLINE_PAGE_DIRECTORY_ADDRESS), (uint64_t)pageDirectory);
    WRITE_64(vmm_physical_to_virtual(SMP_TRAMPOLINE_ENTRY_ADDRESS), smp_entry);
}

void smp_trampoline_cpu_setup(Cpu* cpu)
{
    WRITE_64(vmm_physical_to_virtual(SMP_TRAMPOLINE_STACK_TOP_ADDRESS), cpu->idleStackTop);
}

void smp_trampoline_cleanup()
{   
    memcpy(vmm_physical_to_virtual(SMP_TRAMPOLINE_PHYSICAL_START), backupBuffer, PAGE_SIZE);
    kfree(backupBuffer);

    page_directory_free(pageDirectory);
}