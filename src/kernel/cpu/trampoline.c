#include "trampoline.h"

#include "log/panic.h"
#include "mem/pmm.h"
#include "mem/vmm.h"
#include "utils/utils.h"

#include <common/paging_types.h>
#include <string.h>

static void* backupBuffer;

void trampoline_init(void)
{
    backupBuffer = pmm_alloc();
    if (backupBuffer == NULL)
    {
        panic(NULL, "Failed to allocate memory for trampoline backup");
    }

    memcpy(backupBuffer, TRAMPOLINE_PHYSICAL_START, PAGE_SIZE);
    memcpy(TRAMPOLINE_PHYSICAL_START, trampoline_virtual_start, PAGE_SIZE);

    WRITE_64(TRAMPOLINE_PML_ADDRESS, PML_ENSURE_LOWER_HALF(vmm_kernel_pml()->pml4));
    WRITE_64(TRAMPOLINE_ENTRY_ADDRESS, smp_entry);
}

void trampoline_cpu_setup(uint64_t rsp)
{
    WRITE_64(TRAMPOLINE_STACK_TOP_ADDRESS, rsp);
}

void trampoline_deinit(void)
{
    memcpy(TRAMPOLINE_PHYSICAL_START, backupBuffer, PAGE_SIZE);
    pmm_free(backupBuffer);
}
