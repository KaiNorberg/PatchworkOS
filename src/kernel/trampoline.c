#include "trampoline.h"

#include "utils.h"
#include "vmm.h"

#include <stdlib.h>
#include <string.h>

static void* backupBuffer;
static space_t space;

void trampoline_init(void)
{
    backupBuffer = malloc(PAGE_SIZE);
    memcpy(backupBuffer, VMM_LOWER_TO_HIGHER(TRAMPOLINE_PHYSICAL_START), PAGE_SIZE);
    memcpy(VMM_LOWER_TO_HIGHER(TRAMPOLINE_PHYSICAL_START), trampoline_virtual_start, PAGE_SIZE);

    space_init(&space);
    pml_map(space.pml, TRAMPOLINE_PHYSICAL_START, TRAMPOLINE_PHYSICAL_START, 1, PAGE_WRITE);

    WRITE_64(VMM_LOWER_TO_HIGHER(TRAMPOLINE_PML_ADDRESS), VMM_HIGHER_TO_LOWER(space.pml));
    WRITE_64(VMM_LOWER_TO_HIGHER(TRAMPOLINE_ENTRY_ADDRESS), smp_entry);
}

void trampoline_cpu_setup(cpu_t* cpu)
{
    WRITE_64(VMM_LOWER_TO_HIGHER(TRAMPOLINE_STACK_TOP_ADDRESS), (uint64_t)cpu->idleStack + CPU_IDLE_STACK_SIZE);
}

void trampoline_cleanup(void)
{
    memcpy(VMM_LOWER_TO_HIGHER(TRAMPOLINE_PHYSICAL_START), backupBuffer, PAGE_SIZE);
    free(backupBuffer);

    pml_unmap(space.pml, TRAMPOLINE_PHYSICAL_START, 1);
    space_cleanup(&space);
}
