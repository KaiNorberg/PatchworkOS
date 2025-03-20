#include "space.h"

#include "log.h"
#include "pmm.h"
#include "utils.h"
#include "vmm.h"

void space_init(space_t* space)
{
    space->pml = pml_new();
    space->freeAddress = 0x400000;
    lock_init(&space->lock);

    pml_t* kernelPml = vmm_kernel_pml();
    for (uint64_t i = PAGE_ENTRY_AMOUNT / 2; i < PAGE_ENTRY_AMOUNT; i++)
    {
        space->pml->entries[i] = kernelPml->entries[i];
    }
}

void space_deinit(space_t* space)
{
    for (uint64_t i = PAGE_ENTRY_AMOUNT / 2; i < PAGE_ENTRY_AMOUNT; i++)
    {
        space->pml->entries[i] = (pml_entry_t){0};
    }

    pml_free(space->pml);
}

void space_load(space_t* space)
{
    if (space == NULL)
    {
        pml_load(vmm_kernel_pml());
    }
    else
    {
        pml_load(space->pml);
    }
}
