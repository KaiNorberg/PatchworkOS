#include "smp.h"

#include "acpi/madt.h"
#include "apic.h"
#include "cpu/vectors.h"
#include "drivers/systime/hpet.h"
#include "kernel.h"
#include "log/log.h"
#include "log/panic.h"
#include "mem/heap.h"
#include "sched/thread.h"
#include "trampoline.h"
#include "trap.h"

#include <common/defs.h>
#include <common/regs.h>

#include <assert.h>
#include <stdatomic.h>
#include <stdint.h>

static cpu_t bootstrapCpu;
static cpu_t* cpus[SMP_CPU_MAX];
static uint16_t cpuAmount = 0;
static bool isReady = false;

static atomic_uint16_t haltedAmount = ATOMIC_VAR_INIT(0);

static void cpu_init(cpu_t* cpu, uint8_t id, uint8_t lapicId, bool isBootstrap)
{
    cpu->id = id;
    cpu->lapicId = lapicId;
    cpu->trapDepth = 0;
    cpu->isBootstrap = isBootstrap;
    tss_init(&cpu->tss);
    cli_ctx_init(&cpu->cli);
    sched_cpu_ctx_init(&cpu->sched, cpu);
    wait_cpu_ctx_init(&cpu->wait);
    statistics_cpu_ctx_init(&cpu->stat);
}

static uint64_t cpu_start(cpu_t* cpu)
{
    isReady = false;

    assert(cpu->sched.idleThread != NULL);

    trampoline_cpu_setup(THREAD_KERNEL_STACK_TOP(cpu->sched.idleThread));

    lapic_send_init(cpu->lapicId);
    hpet_sleep(CLOCKS_PER_SEC / 100);
    lapic_send_sipi(cpu->lapicId, ((uint64_t)TRAMPOLINE_PHYSICAL_START) / PAGE_SIZE);

    clock_t timeout = CLOCKS_PER_SEC * 100;
    while (!isReady)
    {
        clock_t sleepDuration = CLOCKS_PER_SEC / 1000;
        hpet_sleep(sleepDuration);
        timeout -= sleepDuration;
        if (timeout == 0)
        {
            return ERR;
        }
    }

    return 0;
}

void smp_bootstrap_init(void)
{
    cpuAmount = 1;
    cpus[0] = &bootstrapCpu;
    cpu_init(cpus[0], 0, 0, true);

    msr_write(MSR_CPU_ID, cpus[0]->id);
}

void smp_others_init(void)
{
    trampoline_init();

    cpuid_t newId = 1;
    uint8_t lapicId = lapic_self_id();

    cpus[0]->lapicId = lapicId;
    LOG_INFO("bootstrap cpu, ready\n", (uint64_t)cpus[0]->id);

    madt_lapic_t* record;
    MADT_FOR_EACH(madt_get(), record)
    {
        if (record->header.type != MADT_LAPIC)
        {
            continue;
        }

        if (record->flags & MADT_LAPIC_ENABLED && lapicId != record->id)
        {
            cpuid_t id = newId++;
            cpus[id] = heap_alloc(sizeof(cpu_t), HEAP_NONE);
            if (cpus[id] == NULL)
            {
                panic(NULL, "Failed to allocate memory for cpu\n");
            }
            cpu_init(cpus[id], id, record->id, false);
            cpuAmount++;
            if (cpu_start(cpus[id]) == ERR)
            {
                panic(NULL, "Failed to start cpu\n");
            }
        }
    }

    trampoline_deinit();
}

void smp_entry(void)
{
    cpu_t* cpu = smp_self_brute();
    msr_write(MSR_CPU_ID, cpu->id);

    kernel_other_init();

    LOG_INFO("ready\n", (uint64_t)cpu->id);
    isReady = true;

    sched_idle_loop();
}

static void smp_halt_ipi(trap_frame_t* trapFrame)
{
    atomic_fetch_add(&haltedAmount, 1);

    while (1)
    {
        asm volatile("cli");
        asm volatile("hlt");
    }
}

void smp_halt_others(void)
{
    const cpu_t* self = smp_self_unsafe();
    for (uint8_t id = 0; id < cpuAmount; id++)
    {
        if (self->id != id)
        {
            lapic_send_ipi(cpus[id]->lapicId, VECTOR_HALT);
        }
    }
}

void smp_notify(cpu_t* cpu)
{
    lapic_send_ipi(cpu->lapicId, VECTOR_NOTIFY);
}

uint8_t smp_cpu_amount(void)
{
    return cpuAmount;
}

cpu_t* smp_cpu(uint8_t id)
{
    return cpus[id];
}

cpu_t* smp_self_unsafe(void)
{
    assert(!(rflags_read() & RFLAGS_INTERRUPT_ENABLE));

    return cpus[msr_read(MSR_CPU_ID)];
}

cpu_t* smp_self_brute(void)
{
    assert(!(rflags_read() & RFLAGS_INTERRUPT_ENABLE));

    uint8_t lapicId = lapic_self_id();
    for (uint16_t id = 0; id < cpuAmount; id++)
    {
        if (cpus[id]->lapicId == lapicId)
        {
            return cpus[id];
        }
    }

    panic(NULL, "Unable to find cpu");
}

cpu_t* smp_self(void)
{
    cli_push();

    return cpus[msr_read(MSR_CPU_ID)];
}

void smp_put(void)
{
    cli_pop();
}
