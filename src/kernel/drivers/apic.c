#include "apic.h"

#include "acpi/madt.h"
#include "cpu/smp.h"
#include "drivers/pic.h"
#include "drivers/time/hpet.h"
#include "log/log.h"
#include "log/panic.h"
#include "mem/vmm.h"
#include "sched/timer.h"
#include "utils/utils.h"

#include <assert.h>
#include <common/defs.h>

static uintptr_t lapicBase;

static ioapic_t ioapics[SMP_CPU_MAX];
static uint32_t ioapicCount = 0;

void apic_timer_one_shot(uint8_t vector, uint32_t ticks)
{
    lapic_write(LAPIC_REG_LVT_TIMER, APIC_TIMER_MASKED);

    lapic_write(LAPIC_REG_TIMER_DIVIDER, APIC_TIMER_DEFAULT_DIV);
    lapic_write(LAPIC_REG_LVT_TIMER, ((uint32_t)vector) | APIC_TIMER_ONE_SHOT);
    lapic_write(LAPIC_REG_TIMER_INITIAL_COUNT, ticks);
}

uint64_t apic_timer_ticks_per_ns(void)
{
    cli_push();

    lapic_write(LAPIC_REG_TIMER_DIVIDER, APIC_TIMER_DEFAULT_DIV);
    lapic_write(LAPIC_REG_LVT_TIMER, APIC_TIMER_MASKED);
    lapic_write(LAPIC_REG_TIMER_INITIAL_COUNT, UINT32_MAX);

    hpet_wait(CLOCKS_PER_SEC / 1000);

    lapic_write(LAPIC_REG_LVT_TIMER, APIC_TIMER_MASKED);

    uint64_t ticks = UINT32_MAX - lapic_read(LAPIC_REG_TIMER_CURRENT_COUNT);
    lapic_write(LAPIC_REG_LVT_TIMER, APIC_TIMER_MASKED);
    lapic_write(LAPIC_REG_TIMER_INITIAL_COUNT, 0);

    uint64_t ticksPerNs = (ticks << APIC_TIMER_TICKS_FIXED_POINT_OFFSET) / 10000000ULL;

    LOG_DEBUG("timer calibration ticks=%llu ticks_per_ns=%llu\n", ticks, ticksPerNs);
    cli_pop();
    return ticksPerNs;
}

void lapic_init(void)
{
    void* lapicPhysAddr = madt_lapic_address();
    if (lapicPhysAddr == NULL)
    {
        panic(NULL, "Unable to find lapic address in MADT, hardware is not compatible");
    }

    lapicBase = (uintptr_t)vmm_kernel_map(NULL, lapicPhysAddr, 1, PML_WRITE);
    if ((void*)lapicBase == NULL)
    {
        panic(NULL, "Unable to map lapic");
    }

    LOG_INFO("local apic mapped base=0x%016lx phys=0x%016lx\n", lapicBase, lapicPhysAddr);
}

void lapic_cpu_init(void)
{
    uint64_t lapicMsr = msr_read(MSR_LAPIC);
    msr_write(MSR_LAPIC, (lapicMsr | LAPIC_MSR_ENABLE) & ~LAPIC_MSR_BSP);

    lapic_write(LAPIC_REG_SPURIOUS, lapic_read(LAPIC_REG_SPURIOUS) | LAPIC_SPURIOUS_ENABLE);

    lapic_write(LAPIC_REG_LVT_TIMER, APIC_TIMER_MASKED);
    lapic_write(LAPIC_REG_LVT_ERROR, LAPIC_LVT_MASKED);
    lapic_write(LAPIC_REG_LVT_PERFCTR, LAPIC_LVT_MASKED);
    lapic_write(LAPIC_REG_LVT_THERMAL, LAPIC_LVT_MASKED);
    lapic_write(LAPIC_REG_LVT_LINT0, LAPIC_LVT_MASKED);
    lapic_write(LAPIC_REG_LVT_LINT1, LAPIC_LVT_MASKED);

    lapic_write(LAPIC_REG_TASK_PRIORITY, 0);

    LOG_INFO("local apic initialized id=%d msr=0x%016lx\n", lapic_self_id(), lapicMsr);
}

uint8_t lapic_self_id(void)
{
    return (uint8_t)(lapic_read(LAPIC_REG_ID) >> LAPIC_ID_OFFSET);
}

void lapic_write(uint32_t reg, uint32_t value)
{
    WRITE_32(lapicBase + reg, value);
}

uint32_t lapic_read(uint32_t reg)
{
    return READ_32(lapicBase + reg);
}

void lapic_send_init(uint32_t id)
{
    lapic_write(LAPIC_REG_ICR1, id << LAPIC_ID_OFFSET);
    lapic_write(LAPIC_REG_ICR0, LAPIC_ICR_INIT);
}

void lapic_send_sipi(uint32_t id, void* entryPoint)
{
    assert((uintptr_t)entryPoint % PAGE_SIZE == 0);

    lapic_write(LAPIC_REG_ICR1, id << LAPIC_ID_OFFSET);
    lapic_write(LAPIC_REG_ICR0, LAPIC_ICR_STARTUP | ((uintptr_t)entryPoint / PAGE_SIZE));
}

void lapic_send_ipi(uint32_t id, uint8_t vector)
{
    lapic_write(LAPIC_REG_ICR1, id << LAPIC_ID_OFFSET);
    lapic_write(LAPIC_REG_ICR0, (uint32_t)vector | LAPIC_ICR_CLEAR_INIT_LEVEL);
}

void lapic_eoi(void)
{
    lapic_write(LAPIC_REG_EOI, 0);
}

static uint32_t ioapic_read(ioapic_id_t id, uint32_t reg)
{
    if (id >= ioapicCount || ioapics[id].base == 0)
    {
        panic(NULL, "invalid i/o apic id %u\n", id);
    }

    uintptr_t base = ioapics[id].base;
    WRITE_32(base + IOAPIC_REG_SELECT, reg);
    return READ_32(base + IOAPIC_REG_DATA);
}

static void ioapic_write(ioapic_id_t id, uint32_t reg, uint32_t value)
{
    if (id >= ioapicCount || ioapics[id].base == 0)
    {
        panic(NULL, "invalid i/o apic id %u\n", id);
    }

    uintptr_t base = ioapics[id].base;
    WRITE_32(base + IOAPIC_REG_SELECT, reg);
    WRITE_32(base + IOAPIC_REG_DATA, value);
}

static ioapic_version_t ioapic_get_version(ioapic_id_t id)
{
    ioapic_version_t version;
    version.raw = ioapic_read(id, IOAPIC_REG_VERSION);
    return version;
}

void ioapic_all_init(void)
{
    pic_disable();

    madt_t* madt = madt_get();
    if (madt == NULL)
    {
        LOG_ERR("madt not found, kernel corruption likely\n");
        return;
    }

    madt_ioapic_t* record;
    MADT_FOR_EACH(madt_get(), record)
    {
        if (record->header.type != MADT_IOAPIC)
        {
            continue;
        }

        void* physAddr = (void*)(uint64_t)record->address;
        void* virtAddr = vmm_kernel_map(NULL, physAddr, 1, PML_WRITE);
        if (virtAddr == NULL)
        {
            panic(NULL, "Failed to map ioapic");
        }

        ioapic_id_t id = ioapicCount++;

        ioapics[id].base = (uintptr_t)virtAddr;
        ioapics[id].gsiBase = record->gsiBase;
        ioapics[id].maxRedirs = ioapic_get_version(id).maxRedirs;

        // Mask all interrupts.
        for (uint32_t i = 0; i < ioapics[id].maxRedirs; i++)
        {
            ioapic_redirect_entry_t maskedEntry = {.mask = 1};
            ioapic_write(id, IOAPIC_REG_REDIRECTION(i, 0), maskedEntry.raw.low);
            ioapic_write(id, IOAPIC_REG_REDIRECTION(i, 1), maskedEntry.raw.high);
        }

        LOG_INFO("io apic initialized id=%u base=0x%016lx gsiBase=%u maxRedirs=%u\n", id, ioapics[id].base,
            ioapics[id].gsiBase, ioapics[id].maxRedirs);
    }
}

static ioapic_id_t ioapic_from_gsi(uint32_t gsi)
{
    for (uint32_t i = 0; i < ioapicCount; i++)
    {
        if (ioapics[i].gsiBase <= gsi && ioapics[i].gsiBase + ioapics[i].maxRedirs > gsi)
        {
            return i;
        }
    }

    panic(NULL, "Failed to locate vector for gsi %d", gsi);
}

void ioapic_set_redirect(uint8_t vector, uint32_t gsi, ioapic_delivery_mode_t deliveryMode, ioapic_polarity_t polarity,
    ioapic_trigger_mode_t triggerMode, cpu_t* cpu, bool enable)
{
    ioapic_redirect_entry_t redirect = {
        .vector = vector,
        .deliveryMode = deliveryMode,
        .deliveryStatus = 0,
        .polarity = polarity,
        .remoteIRR = 0,
        .triggerMode = triggerMode,
        .mask = enable ? 0 : 1,
        .destination = cpu->lapicId,
    };

    ioapic_id_t ioapicId = ioapic_from_gsi(gsi);
    uint32_t pin = gsi - ioapics[ioapicId].gsiBase;

    ioapic_write(ioapicId, IOAPIC_REG_REDIRECTION(pin, 0), redirect.raw.low);
    ioapic_write(ioapicId, IOAPIC_REG_REDIRECTION(pin, 1), redirect.raw.high);

    LOG_INFO("ioapic redirect set gsi=%u vector=%u cpu=%u enable=%d\n", gsi, vector, cpu->id, enable);
}
