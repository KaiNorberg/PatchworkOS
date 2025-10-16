#include "trampoline.h"

#include "drivers/hpet.h"
#include "log/log.h"
#include "log/panic.h"
#include "mem/pmm.h"
#include "mem/vmm.h"
#include "utils/utils.h"

#include <common/paging_types.h>
#include <string.h>

static void* backupBuffer;

static atomic_bool cpuReadyFlag;

void trampoline_init(void)
{
    backupBuffer = pmm_alloc();
    if (backupBuffer == NULL)
    {
        panic(NULL, "Failed to allocate memory for trampoline backup");
    }

    assert(TRAMPOLINE_SIZE < PAGE_SIZE);

    memcpy(backupBuffer, (void*)TRAMPOLINE_BASE_ADDR, TRAMPOLINE_SIZE);
    memcpy((void*)TRAMPOLINE_BASE_ADDR, trampoline_start, TRAMPOLINE_SIZE);
    memset(TRAMPOLINE_ADDR(TRAMPOLINE_DATA_OFFSET), 0, PAGE_SIZE - TRAMPOLINE_DATA_OFFSET);

    WRITE_64(TRAMPOLINE_ADDR(TRAMPOLINE_PML4_OFFSET), PML_ENSURE_LOWER_HALF(vmm_get_kernel_space()->pageTable.pml4));
    WRITE_64(TRAMPOLINE_ADDR(TRAMPOLINE_STACK_OFFSET), 0);
    WRITE_64(TRAMPOLINE_ADDR(TRAMPOLINE_ENTRY_OFFSET), 0);
    WRITE_64(TRAMPOLINE_ADDR(TRAMPOLINE_CPU_ID_OFFSET), 0);

    atomic_init(&cpuReadyFlag, false);

    LOG_DEBUG("trampoline initialized\n");
}

uint64_t trampoline_cpu_setup(cpuid_t cpuId, uintptr_t stackTop, void (*entry)(cpuid_t))
{
    WRITE_64(TRAMPOLINE_ADDR(TRAMPOLINE_STACK_OFFSET), stackTop);
    WRITE_64(TRAMPOLINE_ADDR(TRAMPOLINE_ENTRY_OFFSET), (uint64_t)entry);
    WRITE_64(TRAMPOLINE_ADDR(TRAMPOLINE_CPU_ID_OFFSET), (uint64_t)cpuId);

    atomic_store(&cpuReadyFlag, false);
    return 0;
}

uint64_t trampoline_wait_ready(cpuid_t cpuId, clock_t timeout)
{
    clock_t elapsed = 0;

    while (elapsed < timeout)
    {
        if (atomic_load(&cpuReadyFlag))
        {
            LOG_INFO("cpu%d ready after %u ms\n", cpuId, elapsed / (CLOCKS_PER_SEC / 1000));
            return 0;
        }

        hpet_wait(CLOCKS_PER_SEC / 10000);
        elapsed += CLOCKS_PER_SEC / 10000;
    }

    return ERR;
}

void trampoline_signal_ready(void)
{
    atomic_store(&cpuReadyFlag, true);
}

void trampoline_deinit(void)
{
    memcpy((void*)TRAMPOLINE_BASE_ADDR, backupBuffer, PAGE_SIZE);

    pmm_free(backupBuffer);
    backupBuffer = NULL;

    LOG_DEBUG("trampoline deinitialized\n");
}
