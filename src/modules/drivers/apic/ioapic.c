#include <modules/drivers/apic/ioapic.h>
#include <modules/drivers/apic/lapic.h>

#include <kernel/cpu/cpu.h>
#include <kernel/cpu/irq.h>
#include <kernel/log/log.h>
#include <kernel/mem/vmm.h>
#include <kernel/utils/utils.h>
#include <modules/acpi/tables.h>

#include <assert.h>
#include <sys/defs.h>

static uint32_t ioapic_read(ioapic_t* ioapic, ioapic_register_t reg)
{
    uintptr_t base = PML_LOWER_TO_HIGHER(ioapic->ioApicAddress);
    WRITE_32(base + IOAPIC_MMIO_REG_SELECT, reg);
    return READ_32(base + IOAPIC_MMIO_REG_DATA);
}

static void ioapic_write(ioapic_t* ioapic, ioapic_register_t reg, uint32_t value)
{
    uintptr_t base = PML_LOWER_TO_HIGHER(ioapic->ioApicAddress);
    WRITE_32(base + IOAPIC_MMIO_REG_SELECT, reg);
    WRITE_32(base + IOAPIC_MMIO_REG_DATA, value);
}

static ioapic_version_t ioapic_version_read(ioapic_t* ioapic)
{
    ioapic_version_t version;
    version.raw = ioapic_read(ioapic, IOAPIC_REG_VERSION);
    return version;
}

static void ioapic_redirect_write(ioapic_t* ioapic, ioapic_gsi_t gsi, ioapic_redirect_entry_t entry)
{
    assert(ioapic != NULL);
    assert(gsi >= ioapic->globalSystemInterruptBase);
    assert(gsi < ioapic->globalSystemInterruptBase + ioapic_version_read(ioapic).maxRedirs);

    uint32_t pin = gsi - ioapic->globalSystemInterruptBase;
    ioapic_write(ioapic, IOAPIC_REG_REDIRECTION_BASE + (pin * 2), entry.raw.low);
    ioapic_write(ioapic, IOAPIC_REG_REDIRECTION_BASE + (pin * 2) + 1, entry.raw.high);
}

static uint64_t ioapic_enable(irq_t* irq)
{
    ioapic_t* ioapic = irq->domain->private;
    lapic_t* lapic = lapic_get(cpu_get_id_unsafe());

    ioapic_redirect_entry_t redirect = {
        .vector = irq->virt,
        .deliveryMode = IOAPIC_DELIVERY_NORMAL,
        .destinationMode = IOAPIC_DESTINATION_PHYSICAL,
        .deliveryStatus = 0,
        .polarity = (irq->flags & IRQ_POLARITY_LOW) ? IOAPIC_POLARITY_LOW : IOAPIC_POLARITY_HIGH,
        .remoteIRR = 0,
        .triggerMode = irq->flags & IRQ_TRIGGER_EDGE ? IOAPIC_TRIGGER_EDGE : IOAPIC_TRIGGER_LEVEL,
        .mask = 0,
        .destination = lapic->lapicId,
    };

    ioapic_redirect_write(ioapic, irq->phys, redirect);
    return 0;
}

static void ioapic_disable(irq_t* irq)
{
    ioapic_t* ioapic = irq->domain->private;

    ioapic_redirect_entry_t redirect = {.mask = 1};

    ioapic_redirect_write(ioapic, irq->phys, redirect);
}

static void ioapic_eoi(irq_t* irq)
{
    UNUSED(irq);

    lapic_write(LAPIC_REG_EOI, 0);
}

static irq_chip_t ioApicChip = {
    .name = "IO APIC",
    .enable = ioapic_enable,
    .disable = ioapic_disable,
    .ack = NULL,
    .eoi = ioapic_eoi,
};

uint64_t ioapic_all_init(void)
{
    madt_t* madt = (madt_t*)acpi_tables_lookup(MADT_SIGNATURE, sizeof(madt_t), 0);
    if (madt == NULL)
    {
        LOG_ERR("no MADT table found\n");
        return ERR;
    }

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

        uint32_t maxRedirs = ioapic_version_read(ioapic).maxRedirs;

        LOG_INFO("found I/O APIC globalSystemInterruptBase=0x%02x maxRedirs=0x%02x\n",
            ioapic->globalSystemInterruptBase, maxRedirs);

        // Mask all interrupts.
        for (uint32_t i = ioapic->globalSystemInterruptBase; i < ioapic->globalSystemInterruptBase + maxRedirs; i++)
        {
            ioapic_redirect_entry_t maskedEntry = {.mask = 1};
            ioapic_redirect_write(ioapic, i, maskedEntry);
        }

        if (irq_chip_register(&ioApicChip, ioapic->globalSystemInterruptBase,
                ioapic->globalSystemInterruptBase + maxRedirs, ioapic) == ERR)
        {
            LOG_ERR("failed to register io apic irq chip\n");
            return ERR;
        }
    }

    return 0;
}
