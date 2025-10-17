#include "trampoline.h"

#include "cpu.h"
#include "drivers/apic.h"
#include "drivers/hpet.h"
#include "log/log.h"
#include "log/panic.h"
#include "mem/pmm.h"
#include "mem/vmm.h"
#include "sched/sched.h"
#include "sched/thread.h"
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

    atomic_init(&cpuReadyFlag, false);

    LOG_DEBUG("trampoline initialized\n");
}

void trampoline_send_startup_ipi(cpu_t* cpu)
{
    WRITE_64(TRAMPOLINE_ADDR(TRAMPOLINE_STACK_OFFSET), cpu->interruptStack.top);
    atomic_store(&cpuReadyFlag, false);

    lapic_send_init(cpu->lapicId);
    hpet_wait(CLOCKS_PER_SEC / 100);
    lapic_send_sipi(cpu->lapicId, (void*)TRAMPOLINE_BASE_ADDR);
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
