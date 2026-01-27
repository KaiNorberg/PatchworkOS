#include <kernel/drivers/apic/lapic.h>

#include <kernel/acpi/tables.h>
#include <kernel/cpu/cpu.h>
#include <kernel/cpu/ipi.h>
#include <kernel/cpu/irq.h>
#include <kernel/log/log.h>
#include <kernel/mem/vmm.h>
#include <kernel/utils/utils.h>

#include <assert.h>
#include <sys/defs.h>

static void* lapicBase = NULL;

PERCPU_DEFINE_CTOR(lapic_t, _pcpu_lapic)
{
    // Enable the local apic, enable spurious interrupts and mask everything for now.

    uint64_t lapicMsr = msr_read(MSR_LAPIC);
    msr_write(MSR_LAPIC, (lapicMsr | LAPIC_MSR_ENABLE) & ~LAPIC_MSR_BSP);

    lapic_write(LAPIC_REG_SPURIOUS, VECTOR_SPURIOUS | LAPIC_SPURIOUS_ENABLE);

    lapic_write(LAPIC_REG_LVT_TIMER, LAPIC_LVT_MASKED);
    lapic_write(LAPIC_REG_LVT_ERROR, LAPIC_LVT_MASKED);
    lapic_write(LAPIC_REG_LVT_PERFCTR, LAPIC_LVT_MASKED);
    lapic_write(LAPIC_REG_LVT_THERMAL, LAPIC_LVT_MASKED);
    lapic_write(LAPIC_REG_LVT_LINT0, LAPIC_LVT_MASKED);
    lapic_write(LAPIC_REG_LVT_LINT1, LAPIC_LVT_MASKED);

    lapic_write(LAPIC_REG_TASK_PRIORITY, 0);

    _pcpu_lapic->lapicId = (lapic_id_t)(lapic_read(LAPIC_REG_ID) >> LAPIC_REG_ID_OFFSET);
}

uint32_t lapic_read(uint32_t reg)
{
    return READ_32(lapicBase + reg);
}

void lapic_write(uint32_t reg, uint32_t value)
{
    WRITE_32(lapicBase + reg, value);
}

static void lapic_interrupt(cpu_t* cpu, irq_virt_t virt)
{
    lapic_t* local = CPU_PTR(cpu->id, _pcpu_lapic);

    lapic_write(LAPIC_REG_ICR1, local->lapicId << LAPIC_REG_ID_OFFSET);
    lapic_write(LAPIC_REG_ICR0, (uint32_t)virt | LAPIC_ICR_FIXED);
}

static void lapic_eoi(void)
{
    lapic_write(LAPIC_REG_EOI, 0);
}

static ipi_chip_t lapicIpiChip = {
    .name = "Local APIC IPI",
    .interrupt = lapic_interrupt,
    .eoi = lapic_eoi,
};

void lapic_send_init(lapic_id_t id)
{
    lapic_write(LAPIC_REG_ICR1, id << LAPIC_REG_ID_OFFSET);
    lapic_write(LAPIC_REG_ICR0, LAPIC_ICR_INIT);
}

void lapic_send_sipi(lapic_id_t id, void* entryPoint)
{
    assert((uintptr_t)entryPoint % PAGE_SIZE == 0);

    lapic_write(LAPIC_REG_ICR1, id << LAPIC_REG_ID_OFFSET);
    lapic_write(LAPIC_REG_ICR0, LAPIC_ICR_STARTUP | ((uintptr_t)entryPoint / PAGE_SIZE));
}

status_t lapic_global_init(void)
{
    madt_t* madt = (madt_t*)acpi_tables_lookup(MADT_SIGNATURE, sizeof(madt_t), 0);
    if (madt == NULL)
    {
        LOG_ERR("no MADT table found\n");
        return ERR(DRIVER, NO_ACPI_TABLE);
    }

    if (madt->header.length < sizeof(madt_t))
    {
        LOG_ERR("madt table too small\n");
        return ERR(DRIVER, INVAL);
    }

    if (madt->localInterruptControllerAddress == (uintptr_t)NULL)
    {
        LOG_ERR("madt has invalid lapic address\n");
        return ERR(DRIVER, INVAL);
    }

    lapicBase = (void*)PML_LOWER_TO_HIGHER(madt->localInterruptControllerAddress);
    status_t status = vmm_map(NULL, &lapicBase, madt->localInterruptControllerAddress, PAGE_SIZE,
            PML_WRITE | PML_GLOBAL | PML_PRESENT, NULL, NULL);
    if (IS_ERR(status))
    {
        LOG_ERR("failed to map local apic\n");
        return status;
    }

    LOG_INFO("local apic mapped base=%p phys=%p\n", lapicBase, (uintptr_t)madt->localInterruptControllerAddress);

    status = ipi_chip_register(&lapicIpiChip);
    if (IS_ERR(status))
    {
        vmm_unmap(NULL, lapicBase, PAGE_SIZE);
        LOG_ERR("failed to register lapic ipi chip\n");
        return status;
    }

    return OK;
}