#include "smp.h"

#include "apic.h"
#include "futex.h"
#include "gdt.h"
#include "hpet.h"
#include "idt.h"
#include "lock.h"
#include "log.h"
#include "madt.h"
#include "regs.h"
#include "sched.h"
#include "trampoline.h"
#include "trap.h"
#include "vmm.h"

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static cpu_t* cpus[UINT8_MAX];
static uint8_t cpuAmount = 0;
static bool cpuReady = false;

static bool initialized = false;

atomic_uint8 haltedAmount = ATOMIC_VAR_INIT(0);

static void ipi_queue_init(ipi_queue_t* queue)
{
    queue->readIndex = 0;
    queue->writeIndex = 0;
    lock_init(&queue->lock);
}

static void ipi_queue_push(ipi_queue_t* queue, ipi_t ipi)
{
    queue->ipis[queue->writeIndex] = ipi;
    queue->writeIndex = (queue->writeIndex + 1) % IPI_QUEUE_MAX;
}

static ipi_t ipi_queue_pop(ipi_queue_t* queue)
{
    if (queue->writeIndex == queue->readIndex)
    {
        return NULL;
    }

    ipi_t ipi = queue->ipis[queue->readIndex];
    queue->readIndex = (queue->readIndex + 1) % IPI_QUEUE_MAX;
    return ipi;
}

static NOINLINE void cpu_init(cpu_t* cpu, uint8_t id, uint8_t lapicId)
{
    cpu->id = id;
    cpu->lapicId = lapicId;
    cpu->trapDepth = 0;
    cpu->prevFlags = 0;
    cpu->cliAmount = 0;
    tss_init(&cpu->tss);
    sched_ctx_init(&cpu->sched);
    ipi_queue_init(&cpu->queue);
}

static NOINLINE uint64_t cpu_start(cpu_t* cpu)
{
    cpuReady = false;

    trampoline_cpu_setup(cpu);

    lapic_send_init(cpu->lapicId);
    hpet_sleep(SEC / 100);
    lapic_send_sipi(cpu->lapicId, ((uint64_t)TRAMPOLINE_PHYSICAL_START) / PAGE_SIZE);

    nsec_t timeout = 1000;
    while (!cpuReady)
    {
        hpet_sleep(SEC / 1000);
        timeout--;
        if (timeout == 0)
        {
            return ERR;
        }
    }

    return 0;
}

static NOINLINE void smp_detect_cpus(void)
{
    cpuAmount = 0;

    madt_t* madt = madt_get();
    madt_lapic_t* record;
    MADT_FOR_EACH(madt, record)
    {
        if (record->header.type == MADT_LAPIC && record->flags & MADT_LAPIC_INITABLE)
        {
            cpuAmount++;
        }
    }

    printf("smp: detected %d cpus", (uint64_t)cpuAmount);
}

static NOINLINE void smp_start(void)
{
    uint8_t newId = 1;
    uint8_t lapicId = lapic_id();

    cpus[0]->lapicId = lapicId;
    printf("cpu %d: bootstrap cpu, ready", (uint64_t)cpus[0]->id);

    madt_t* madt = madt_get();
    madt_lapic_t* record;
    MADT_FOR_EACH(madt, record)
    {
        if (record->header.type != MADT_LAPIC)
        {
            continue;
        }

        if (record->flags & MADT_LAPIC_INITABLE && lapicId != record->lapicId)
        {
            uint8_t id = newId++;
            cpus[id] = malloc(sizeof(cpu_t));
            cpu_init(cpus[id], id, record->lapicId);
            ASSERT_PANIC(cpu_start(cpus[id]) != ERR);
        }
    }
}

void smp_init(void)
{
    cpuAmount = 1;
    cpus[0] = malloc(sizeof(cpu_t));
    cpu_init(cpus[0], 0, 0);

    msr_write(MSR_CPU_ID, cpus[0]->id);
    gdt_load_tss(&cpus[0]->tss);

    initialized = true;
    printf("smp: fake bootstrap cpu");
}

void smp_init_others(void)
{
    smp_detect_cpus();

    trampoline_init();
    smp_start();
    trampoline_deinit();
}

void smp_entry(void)
{
    gdt_init();
    idt_init();

    cpu_t* cpu = smp_self_brute();
    msr_write(MSR_CPU_ID, cpu->id);
    gdt_load_tss(&cpu->tss);

    lapic_init();
    simd_init();

    vmm_cpu_init();

    printf("cpu %d: ready", (uint64_t)cpu->id);

    cpuReady = true;
    sched_idle_loop();
}

bool smp_initialized(void)
{
    return initialized;
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
    smp_send_others(smp_halt_ipi);

    while (atomic_load(&haltedAmount) < cpuAmount - 1)
    {
        asm volatile("pause");
    }
}

ipi_t smp_recieve(cpu_t* cpu)
{
    ipi_queue_t* queue = &cpu->queue;

    lock_acquire(&queue->lock);
    ipi_t ipi = ipi_queue_pop(queue);
    lock_release(&queue->lock);

    return ipi;
}

void smp_send(cpu_t* cpu, ipi_t ipi)
{
    ipi_queue_t* queue = &cpu->queue;

    lock_acquire(&queue->lock);
    ipi_queue_push(queue, ipi);
    lock_release(&queue->lock);

    lapic_send_ipi(cpu->lapicId, VECTOR_IPI);
}

void smp_send_self(ipi_t ipi)
{
    ipi_queue_t* queue = &smp_self_unsafe()->queue;

    lock_acquire(&queue->lock);
    ipi_queue_push(queue, ipi);
    lock_release(&queue->lock);

    asm volatile("int %0" ::"i"(VECTOR_IPI));
}

void smp_send_others(ipi_t ipi)
{
    const cpu_t* self = smp_self_unsafe();
    for (uint8_t id = 0; id < cpuAmount; id++)
    {
        if (self->id != id)
        {
            smp_send(cpus[id], ipi);
        }
    }
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
    ASSERT_PANIC((rflags_read() & RFLAGS_INTERRUPT_ENABLE) == 0);

    return cpus[msr_read(MSR_CPU_ID)];
}

cpu_t* smp_self_brute(void)
{
    ASSERT_PANIC((rflags_read() & RFLAGS_INTERRUPT_ENABLE) == 0);

    uint8_t lapicId = lapic_id();
    for (uint16_t id = 0; id < cpuAmount; id++)
    {
        if (cpus[id]->lapicId == lapicId)
        {
            return cpus[id];
        }
    }

    log_panic(NULL, "Unable to find cpu");
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
