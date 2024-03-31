#include "trampoline.h"

#include <string.h>

#include "utils/utils.h"
#include "heap/heap.h"
#include "vmm/vmm.h"
#include "smp/startup/startup.h"

static void* backupBuffer;
static Space space;

//What a mess...

void smp_trampoline_setup(void)
{
    backupBuffer = kmalloc(PAGE_SIZE);
    memcpy(backupBuffer, VMM_LOWER_TO_HIGHER(SMP_TRAMPOLINE_PHYSICAL_START), PAGE_SIZE);

    space_init(&space);
    page_table_map(space.pageTable, SMP_TRAMPOLINE_PHYSICAL_START, SMP_TRAMPOLINE_PHYSICAL_START, PAGE_FLAG_WRITE);

    memcpy(VMM_LOWER_TO_HIGHER(SMP_TRAMPOLINE_PHYSICAL_START), smp_trampoline_virtual_start, PAGE_SIZE);

    WRITE_64(VMM_LOWER_TO_HIGHER(SMP_TRAMPOLINE_PAGE_TABLE_ADDRESS), (uint64_t)VMM_HIGHER_TO_LOWER(space.pageTable));
    WRITE_64(VMM_LOWER_TO_HIGHER(SMP_TRAMPOLINE_ENTRY_ADDRESS), smp_entry);
}

void smp_trampoline_cpu_setup(Cpu* cpu)
{
    WRITE_64(VMM_LOWER_TO_HIGHER(SMP_TRAMPOLINE_STACK_TOP_ADDRESS), (uint64_t)cpu->idleStack + CPU_IDLE_STACK_SIZE);
}

void smp_trampoline_cleanup(void)
{   
    memcpy(VMM_LOWER_TO_HIGHER(SMP_TRAMPOLINE_PHYSICAL_START), backupBuffer, PAGE_SIZE);
    kfree(backupBuffer);

    page_table_unmap(space.pageTable, SMP_TRAMPOLINE_PHYSICAL_START);
    space_cleanup(&space);
}