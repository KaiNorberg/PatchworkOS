#include "trampoline.h"

#include <libc/string.h>

#include "utils/utils.h"
#include "heap/heap.h"
#include "vmm/vmm.h"
#include "smp/startup/startup.h"

static void* backupBuffer;
static AddressSpace* addressSpace;

//What a mess...

void smp_trampoline_setup(void)
{
    backupBuffer = kmalloc(PAGE_SIZE);
    memcpy(backupBuffer, vmm_physical_to_virtual(SMP_TRAMPOLINE_PHYSICAL_START), PAGE_SIZE);

    addressSpace = address_space_new();
    page_directory_map(addressSpace->pageDirectory, SMP_TRAMPOLINE_PHYSICAL_START, SMP_TRAMPOLINE_PHYSICAL_START, PAGE_FLAG_WRITE);

    memcpy(vmm_physical_to_virtual(SMP_TRAMPOLINE_PHYSICAL_START), smp_trampoline_virtual_start, PAGE_SIZE);

    WRITE_64(vmm_physical_to_virtual(SMP_TRAMPOLINE_PAGE_DIRECTORY_ADDRESS), (uint64_t)vmm_virtual_to_physical(addressSpace->pageDirectory));
    WRITE_64(vmm_physical_to_virtual(SMP_TRAMPOLINE_ENTRY_ADDRESS), smp_entry);
}

void smp_trampoline_cpu_setup(Cpu* cpu)
{
    WRITE_64(vmm_physical_to_virtual(SMP_TRAMPOLINE_STACK_TOP_ADDRESS), cpu->idleStackTop);
}

void smp_trampoline_cleanup(void)
{   
    memcpy(vmm_physical_to_virtual(SMP_TRAMPOLINE_PHYSICAL_START), backupBuffer, PAGE_SIZE);
    kfree(backupBuffer);

    address_space_free(addressSpace);
}