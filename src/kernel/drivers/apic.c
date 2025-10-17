#include "apic.h"

#include "acpi/tables.h"
#include "cpu/smp.h"
#include "drivers/hpet.h"
#include "drivers/pic.h"
#include "log/log.h"
#include "log/panic.h"
#include "mem/vmm.h"
#include "sched/timer.h"
#include "utils/utils.h"

#include <assert.h>
#include <common/defs.h>

static bool initialized = false;
static madt_t* madt;
static uintptr_t lapicBase;

static uint64_t lapic_init(void)
{
    void* lapicPhysAddr = (void*)(uint64_t)madt->localInterruptControllerAddress;
    if (lapicPhysAddr == NULL)
    {
        LOG_ERR("madt has invalid lapic address\n");
        return ERR;
    }

    lapicBase = (uintptr_t)PML_LOWER_TO_HIGHER(lapicPhysAddr);
    if (vmm_map(NULL, (void*)lapicBase, lapicPhysAddr, PAGE_SIZE, PML_WRITE | PML_GLOBAL | PML_PRESENT, NULL, NULL) ==
        NULL)
    {
        LOG_ERR("failed to map local apic\n");
        return ERR;
    }

    LOG_INFO("local apic mapped base=0x%016lx phys=0x%016lx\n", lapicBase, lapicPhysAddr);
    return 0;
}

static uint64_t ioapic_all_init(void)
{
    pic_disable();

    ioapic_t* ioapic;
    MADT_FOR_EACH(madt, ioapic)
    {
        if (ioapic->header.type != INTERRUPT_CONTROLLER_IO_APIC)
        {
            continue;
        }

        void* physAddr = (void*)(uint64_t)ioapic->ioApicAddress;
        void* virtAddr = (void*)PML_LOWER_TO_HIGHER(physAddr);
        if (vmm_map(NULL, virtAddr, physAddr, PAGE_SIZE, PML_WRITE | PML_GLOBAL | PML_PRESENT, NULL, NULL) == NULL)
        {
            LOG_ERR("failed to map io apic\n");
            return ERR;
        }

        uint32_t maxRedirs = ioapic_get_version(ioapic).maxRedirs;
        // Mask all interrupts.
        for (uint32_t i = 0; i < maxRedirs; i++)
        {
            ioapic_redirect_entry_t maskedEntry = {.mask = 1};
            ioapic_write(ioapic, IOAPIC_REG_REDIRECTION(i, 0), maskedEntry.raw.low);
            ioapic_write(ioapic, IOAPIC_REG_REDIRECTION(i, 1), maskedEntry.raw.high);
        }

        LOG_INFO("io apic initialized base=0x%016lx gsiBase=%u maxRedirs=%u\n", virtAddr,
            ioapic->globalSystemInterruptBase, maxRedirs);
    }

    return 0;
}

static uint64_t apic_init(sdt_header_t* table)
{
    madt = (madt_t*)table;
    if (initialized)
    {
        LOG_ERR("multiple MADT tables found\n");
        return ERR;
    }
    initialized = true;

    if (lapic_init() == ERR)
    {
        LOG_ERR("failed to initialize local apic\n");
        return ERR;
    }

    if (ioapic_all_init() == ERR)
    {
        LOG_ERR("failed to initialize ioapics\n");
        return ERR;
    }

    return 0;
}

ACPI_SDT_HANDLER_REGISTER(MADT_SIGNATURE, apic_init);

void apic_timer_one_shot(vector_t vector, uint32_t ticks)
{
    if (!initialized)
    {
        panic(NULL, "apic timer used before apic initialized");
    }

    lapic_write(LAPIC_REG_LVT_TIMER, APIC_TIMER_MASKED);

    lapic_write(LAPIC_REG_TIMER_DIVIDER, APIC_TIMER_DIV_DEFAULT);
    lapic_write(LAPIC_REG_LVT_TIMER, ((uint32_t)vector) | APIC_TIMER_ONE_SHOT);
    lapic_write(LAPIC_REG_TIMER_INITIAL_COUNT, ticks);
}

uint64_t apic_timer_ticks_per_ns(void)
{
    if (!initialized)
    {
        panic(NULL, "apic timer calibration used before apic initialized");
    }

    interrupt_disable();

    lapic_write(LAPIC_REG_TIMER_DIVIDER, APIC_TIMER_DIV_DEFAULT);
    lapic_write(LAPIC_REG_LVT_TIMER, APIC_TIMER_MASKED);
    lapic_write(LAPIC_REG_TIMER_INITIAL_COUNT, UINT32_MAX);

    hpet_wait(CLOCKS_PER_SEC / 1000);

    lapic_write(LAPIC_REG_LVT_TIMER, APIC_TIMER_MASKED);

    uint64_t ticks = UINT32_MAX - lapic_read(LAPIC_REG_TIMER_CURRENT_COUNT);
    lapic_write(LAPIC_REG_LVT_TIMER, APIC_TIMER_MASKED);
    lapic_write(LAPIC_REG_TIMER_INITIAL_COUNT, 0);

    uint64_t ticksPerNs = (ticks << APIC_TIMER_TICKS_FIXED_POINT_OFFSET) / 10000000ULL;

    LOG_DEBUG("timer calibration ticks=%llu ticks_per_ns=%llu\n", ticks, ticksPerNs);
    interrupt_enable();
    return ticksPerNs;
}

void lapic_cpu_init(void)
{
    if (!initialized)
    {
        panic(NULL, "local apic used before apic initialized");
    }

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

    LOG_INFO("cpu%d local apic id=%d msr=0x%016lx\n", smp_self_unsafe()->id, lapic_self_id(), lapicMsr);
}

lapic_id_t lapic_self_id(void)
{
    if (!initialized)
    {
        panic(NULL, "local apic used before apic initialized");
    }

    return (uint8_t)(lapic_read(LAPIC_REG_ID) >> LAPIC_REG_ICR1_ID_OFFSET);
}

void lapic_write(lapic_register_t reg, uint32_t value)
{
    if (!initialized)
    {
        panic(NULL, "local apic used before apic initialized");
    }

    WRITE_32(lapicBase + reg, value);
}

uint32_t lapic_read(lapic_register_t reg)
{
    if (!initialized)
    {
        panic(NULL, "local apic used before apic initialized");
    }

    return READ_32(lapicBase + reg);
}

void lapic_send_init(lapic_id_t id)
{
    if (!initialized)
    {
        panic(NULL, "local apic used before apic initialized");
    }

    lapic_write(LAPIC_REG_ICR1, id << LAPIC_REG_ICR1_ID_OFFSET);
    lapic_write(LAPIC_REG_ICR0, LAPIC_ICR_INIT);
}

void lapic_send_sipi(lapic_id_t id, void* entryPoint)
{
    if (!initialized)
    {
        panic(NULL, "local apic used before apic initialized");
    }

    assert((uintptr_t)entryPoint % PAGE_SIZE == 0);

    lapic_write(LAPIC_REG_ICR1, id << LAPIC_REG_ICR1_ID_OFFSET);
    lapic_write(LAPIC_REG_ICR0, LAPIC_ICR_STARTUP | ((uintptr_t)entryPoint / PAGE_SIZE));
}

void lapic_send_ipi(lapic_id_t id, vector_t vector)
{
    if (!initialized)
    {
        panic(NULL, "local apic used before apic initialized");
    }

    lapic_write(LAPIC_REG_ICR1, id << LAPIC_REG_ICR1_ID_OFFSET);
    lapic_write(LAPIC_REG_ICR0, (uint32_t)vector | LAPIC_ICR_CLEAR_INIT_LEVEL);
}

void lapic_eoi(void)
{
    if (!initialized)
    {
        panic(NULL, "local apic used before apic initialized");
    }

    lapic_write(LAPIC_REG_EOI, 0);
}

uint32_t ioapic_read(ioapic_t* ioapic, ioapic_register_t reg)
{
    if (!initialized)
    {
        panic(NULL, "ioapic used before apic initialized");
    }

    uintptr_t base = PML_LOWER_TO_HIGHER(ioapic->ioApicAddress);
    WRITE_32(base + IOAPIC_MMIO_REG_SELECT, reg);
    return READ_32(base + IOAPIC_MMIO_REG_DATA);
}

void ioapic_write(ioapic_t* ioapic, ioapic_register_t reg, uint32_t value)
{
    if (!initialized)
    {
        panic(NULL, "ioapic used before apic initialized");
    }

    uintptr_t base = PML_LOWER_TO_HIGHER(ioapic->ioApicAddress);
    WRITE_32(base + IOAPIC_MMIO_REG_SELECT, reg);
    WRITE_32(base + IOAPIC_MMIO_REG_DATA, value);
}

ioapic_version_t ioapic_get_version(ioapic_t* ioapic)
{
    if (!initialized)
    {
        panic(NULL, "ioapic used before apic initialized");
    }

    ioapic_version_t version;
    version.raw = ioapic_read(ioapic, IOAPIC_REG_VERSION);
    return version;
}

ioapic_t* ioapic_from_gsi(ioapic_gsi_t gsi)
{
    if (!initialized)
    {
        panic(NULL, "ioapic used before apic initialized");
    }

    ioapic_t* ioapic;
    MADT_FOR_EACH(madt, ioapic)
    {
        if (ioapic->header.type != INTERRUPT_CONTROLLER_IO_APIC)
        {
            continue;
        }

        ioapic_version_t version = ioapic_get_version(ioapic);
        if (ioapic->globalSystemInterruptBase <= gsi && ioapic->globalSystemInterruptBase + version.maxRedirs > gsi)
        {
            return ioapic;
        }
    }

    panic(NULL, "Failed to locate vector for gsi %d", gsi);
}

void ioapic_set_redirect(vector_t vector, ioapic_gsi_t gsi, ioapic_delivery_mode_t deliveryMode,
    ioapic_polarity_t polarity, ioapic_trigger_mode_t triggerMode, cpu_t* cpu, bool enable)
{
    if (!initialized)
    {
        panic(NULL, "ioapic used before apic initialized");
    }

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

    ioapic_t* ioapic = ioapic_from_gsi(gsi);
    uint32_t pin = gsi - ioapic->globalSystemInterruptBase;

    ioapic_write(ioapic, IOAPIC_REG_REDIRECTION(pin, 0), redirect.raw.low);
    ioapic_write(ioapic, IOAPIC_REG_REDIRECTION(pin, 1), redirect.raw.high);

    LOG_INFO("ioapic redirect set gsi=%u vector=0x%02x cpu=%u enable=%d\n", gsi, vector, cpu->id, enable);
}
