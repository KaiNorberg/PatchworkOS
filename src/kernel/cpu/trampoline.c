#include "trampoline.h"

#include "drivers/hpet.h"
#include "log/log.h"
#include "log/panic.h"
#include "mem/pmm.h"
#include "utils/utils.h"

#include <common/paging_types.h>
#include <string.h>

static void* backupBuffer;

static atomic_bool cpuReadyFlags[SMP_CPU_MAX];

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

    for (uint64_t i = 0; i < SMP_CPU_MAX; i++)
    {
        atomic_store(&cpuReadyFlags[i], false);
    }

    LOG_DEBUG("trampoline initialized\n");
}

uint64_t trampoline_cpu_setup(cpuid_t cpuId, uint64_t stackTop, void (*entry)(cpuid_t))
{
    WRITE_64(TRAMPOLINE_ADDR(TRAMPOLINE_STACK_OFFSET), stackTop);
    WRITE_64(TRAMPOLINE_ADDR(TRAMPOLINE_ENTRY_OFFSET), (uint64_t)entry);
    WRITE_64(TRAMPOLINE_ADDR(TRAMPOLINE_CPU_ID_OFFSET), (uint64_t)cpuId);

    atomic_store(&cpuReadyFlags[cpuId], false);
    return 0;
}

uint64_t trampoline_wait_ready(cpuid_t cpuId, clock_t timeout)
{
    clock_t elapsed = 0;

    while (elapsed < timeout)
    {
        if (atomic_load(&cpuReadyFlags[cpuId]))
        {
            LOG_INFO("cpu%d ready after %u ms\n", cpuId, elapsed / (CLOCKS_PER_SEC / 1000));
            return 0;
        }

        hpet_wait(CLOCKS_PER_SEC / 10000);
        elapsed += CLOCKS_PER_SEC / 10000;
    }

    return ERR;
}

void trampoline_signal_ready(cpuid_t cpuId)
{
    atomic_store(&cpuReadyFlags[cpuId], true);
}

void trampoline_deinit(void)
{
    memcpy((void*)TRAMPOLINE_BASE_ADDR, backupBuffer, PAGE_SIZE);

    pmm_free(backupBuffer);
    backupBuffer = NULL;

    LOG_DEBUG("trampoline deinitialized\n");
}
