#include <kernel/acpi/tables.h>
#include <kernel/cpu/cpu.h>
#include <kernel/cpu/ipi.h>
#include <kernel/cpu/irq.h>
#include <kernel/drivers/pic.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/mem/vmm.h>
#include <kernel/module/module.h>
#include <kernel/sched/sys_time.h>
#include <kernel/sched/timer.h>
#include <kernel/utils/utils.h>

#include <assert.h>
#include <kernel/defs.h>

/**
 * @brief Advanced Programmable Interrupt Controller.
 * @ingroup modules_drivers
 * @defgroup modules_drivers_apic APIC
 *
 * This module implements the Advanced Programmable Interrupt Controller (APIC) driver, which includes both the per-CPU
 * local APICs, the IO APICs and the APIC timer.
 *
 * ## Local APICs
 *
 * Each CPU has its own local APIC in a 1:1 mapping, which, when used with the IO APICs, allows for more advanced
 * interrupt handling such as routing interrupts to specific CPUs, interrupt prioritization, and more. Most of its
 * features are not used in PatchworkOS yet.
 *
 * Additionally, the local APICs provide Inter-Processor Interrupts (IPIs) which allow a CPU to interrupt another CPU by
 * using its local APIC.
 *
 * ## IO APICs
 *
 * The IO APICs are used to route external interrupts to a CPUs local APIC. Each IO APIC handles a range of Global
 * System Interrupts (GSIs) or in PatchworkOS terms, physical IRQs, which it receives from external devices such as a
 * keyboard. The IO APIC then routes these physical IRQs to a local APIC using that local APICs ID, that local APIC then
 * triggers the interrupt on its CPU.
 *
 * So, for example, say we have two IO APICs, 0 and 1, where IO APIC 0 handles physical IRQs 0-23 and IO APIC 1 handles
 * physical IRQs 24-47. Then lets say we want to route physical IRQ 1 to CPU 4. In this case, we would use IO APIC 0 to
 * route physical IRQ 1 to the local APIC ID of CPU 4, lets say this ID is 5. The IO APIC would then send the interrupt
 * to the local APIC with ID 5, which would then trigger the interrupt on CPU 4.
 *
 * The range that each IO APIC handles is defined as the range `[globalSystemInterruptBase, globalSystemInterruptBase +
 * maxRedirs)`, where `globalSystemInterruptBase` is defined in the ACPI MADT table and `maxRedirs` is read from the IO
 * APICs version register.
 *
 * @note The only reason there can be multiple IO APICs is for hardware implementation reasons, things we dont care
 * about. As far as I know, the OS itself does not benefit from having multiple IO APICs.
 *
 * ## APIC Timer
 *
 * Each local APIC also contains a timer which can be used to generate interrupts at specific intervals, or as we use
 * it, to generate a single interrupt after a specified time.
 *
 * @note Its a common mistake to assume that the local APIC IDs are contiguous, or that they are the same as the CPU
 * IDs, but this is not the case. The local APIC IDs are assigned by the firmware and can be any value.
 *
 * @see [ACPI Specification Version 6.6](https://uefi.org/sites/default/files/resources/ACPI_Spec_6.6.pdf)
 * @see [82093AA I/O ADVANCED PROGRAMMABLE INTERRUPT CONTROLLER
 * (IOAPIC)](https://web.archive.org/web/20161130153145/http://download.intel.com/design/chipsets/datashts/29056601.pdf)
 *
 * @{
 */

/**
 * @brief Local APIC ID type.
 */
typedef uint8_t lapic_id_t;

/**
 * @brief APIC Timer Modes.
 * @enum apic_timer_mode_t
 */
typedef enum
{
    APIC_TIMER_MASKED = 0x10000, ///< Timer is masked (disabled)
    APIC_TIMER_PERIODIC = 0x20000,
    APIC_TIMER_ONE_SHOT = 0x00000
} apic_timer_mode_t;

/**
 * @brief APIC Timer Divider Values.
 * @enum apic_timer_divider_t
 */
typedef enum
{
    APIC_TIMER_DIV_16 = 0x3,
    APIC_TIMER_DIV_32 = 0x4,
    APIC_TIMER_DIV_64 = 0x5,
    APIC_TIMER_DIV_128 = 0x6,
    APIC_TIMER_DIV_DEFAULT = APIC_TIMER_DIV_16
} apic_timer_divider_t;

/**
 * @brief Local APIC MSR Flags.
 * @enum lapic_msr_flags_t
 */
typedef enum
{
    LAPIC_MSR_ENABLE = 0x800,
    LAPIC_MSR_BSP = 0x100
} lapic_msr_flags_t;

/**
 * @brief Local APIC Registers.
 * @enum lapic_register_t
 */
typedef enum
{
    LAPIC_REG_ID = 0x020,
    LAPIC_REG_VERSION = 0x030,
    LAPIC_REG_TASK_PRIORITY = 0x080,
    LAPIC_REG_ARBITRATION_PRIORITY = 0x090,
    LAPIC_REG_PROCESSOR_PRIORITY = 0x0A0,
    LAPIC_REG_EOI = 0x0B0,
    LAPIC_REG_REMOTE_READ = 0x0C0,
    LAPIC_REG_LOGICAL_DEST = 0x0D0,
    LAPIC_REG_DEST_FORMAT = 0x0E0,
    LAPIC_REG_SPURIOUS = 0x0F0,
    LAPIC_REG_ISR_BASE = 0x100,
    LAPIC_REG_TMR_BASE = 0x180,
    LAPIC_REG_IRR_BASE = 0x200,
    LAPIC_REG_ERROR_STATUS = 0x280,
    LAPIC_REG_LVT_CMCI = 0x2F0,
    LAPIC_REG_ICR0 = 0x300,
    LAPIC_REG_ICR1 = 0x310,
    LAPIC_REG_LVT_TIMER = 0x320,
    LAPIC_REG_LVT_THERMAL = 0x330,
    LAPIC_REG_LVT_PERFCTR = 0x340,
    LAPIC_REG_LVT_LINT0 = 0x350,
    LAPIC_REG_LVT_LINT1 = 0x360,
    LAPIC_REG_LVT_ERROR = 0x370,
    LAPIC_REG_TIMER_INITIAL_COUNT = 0x380,
    LAPIC_REG_TIMER_CURRENT_COUNT = 0x390,
    LAPIC_REG_TIMER_DIVIDER = 0x3E0
} lapic_register_t;

/**
 * @brief The offset at which the lapic id is stored in the LAPIC_REG_ID register.
 */
#define LAPIC_REG_ID_OFFSET 24

/**
 * @brief Local APIC Flags.
 * @enum lapic_flags_t
 */
typedef enum
{
    LAPIC_SPURIOUS_ENABLE = (1 << 8),
    LAPIC_LVT_MASKED = (1 << 16)
} lapic_flags_t;

/**
 * @brief Local APIC ICR Delivery Modes.
 * @enum lapic_icr_delivery_mode_t
 */
typedef enum
{
    LAPIC_ICR_FIXED = (0 << 8),
    LAPIC_ICR_LOWEST_PRIORITY = (1 << 8),
    LAPIC_ICR_SMI = (2 << 8),
    LAPIC_ICR_NMI = (4 << 8),
    LAPIC_ICR_INIT = (5 << 8),
    LAPIC_ICR_STARTUP = (6 << 8)
} lapic_icr_delivery_mode_t;

/**
 * @brief Local APIC ICR Flags.
 * @enum lapic_icr_flags_t
 */
typedef enum
{
    LAPIC_ICR_CLEAR_INIT_LEVEL = (1 << 14)
} lapic_icr_flags_t;

/**
 * @brief IO APIC Global System Interrupt type.
 */
typedef uint32_t ioapic_gsi_t;

/**
 * @brief IO APIC Memory Mapped Registers.
 * @enum ioapic_mmio_register_t
 */
typedef enum
{
    IOAPIC_MMIO_REG_SELECT = 0x00,
    IOAPIC_MMIO_REG_DATA = 0x10
} ioapic_mmio_register_t;

/**
 * @brief IO APIC Registers.
 * @enum ioapic_register_t
 */
typedef enum
{
    IOAPIC_REG_IDENTIFICATION = 0x00,
    IOAPIC_REG_VERSION = 0x01,
    IOAPIC_REG_ARBITRATION = 0x02,
    IOAPIC_REG_REDIRECTION_BASE = 0x10
} ioapic_register_t;

/**
 * @brief IO APIC Delivery Modes.
 * @enum ioapic_delivery_mode_t
 */
typedef enum
{
    IOAPIC_DELIVERY_NORMAL = 0,
    IOAPIC_DELIVERY_LOW_PRIO = 1,
    IOAPIC_DELIVERY_SMI = 2,
    IOAPIC_DELIVERY_NMI = 4,
    IOAPIC_DELIVERY_INIT = 5,
    IOAPIC_DELIVERY_EXTERNAL = 7
} ioapic_delivery_mode_t;

/**
 * @brief IO APIC Destination Modes.
 * @enum ioapic_destination_mode_t
 */
typedef enum
{
    IOAPIC_DESTINATION_PHYSICAL = 0,
    IOAPIC_DESTINATION_LOGICAL = 1
} ioapic_destination_mode_t;

/**
 * @brief IO APIC Trigger Modes.
 * @enum ioapic_trigger_mode_t
 */
typedef enum
{
    IOAPIC_TRIGGER_EDGE = 0,
    IOAPIC_TRIGGER_LEVEL = 1
} ioapic_trigger_mode_t;

/**
 * @brief IO APIC Polarity Modes.
 * @enum ioapic_polarity_t
 */
typedef enum
{
    IOAPIC_POLARITY_HIGH = 0,
    IOAPIC_POLARITY_LOW = 1
} ioapic_polarity_t;

/**
 * @brief IO APIC Version Structure.
 * @struct ioapic_version_t
 *
 * Stored in the `IOAPIC_REG_VERSION` register.
 */
typedef struct PACKED
{
    union {
        uint32_t raw;
        struct PACKED
        {
            uint8_t version;
            uint8_t reserved;
            uint8_t maxRedirs;
            uint8_t reserved2;
        };
    };
} ioapic_version_t;

/**
 * @brief IO APIC Redirection Entry Structure.
 * @struct ioapic_redirect_entry_t
 *
 * Represents a single redirection entry in the IO APIC.
 */
typedef union {
    struct PACKED
    {
        uint8_t vector;
        uint8_t deliveryMode : 3;    ///< ioapic_delivery_mode_t
        uint8_t destinationMode : 1; ///< ioapic_destination_mode_t
        uint8_t deliveryStatus : 1;
        uint8_t polarity : 1; ///< ioapic_polarity_t
        uint8_t remoteIRR : 1;
        uint8_t triggerMode : 1; ///< ioapic_trigger_mode_t
        uint8_t mask : 1;        ///< If set, the interrupt is masked (disabled)
        uint64_t reserved : 39;
        uint8_t destination : 8;
    };
    struct PACKED
    {
        uint32_t low;
        uint32_t high;
    } raw;
} ioapic_redirect_entry_t;

/**
 * @brief Local APIC Structure.
 * @struct lapic_t
 *
 * Represents each CPU's local APIC and local data.
 */
typedef struct
{
    uint64_t ticksPerMs; ///< Initialized to 0, set on first use of the APIC timer on the CPU.
    lapic_id_t lapicId;
} lapic_t;

/**
 * @brief Cached local apic base address, in the higher half.
 *
 * This address is the same for all CPUs, but each CPU will end up accessing different underlying hardware since each
 * CPU has its own local apic.
 */
static uintptr_t lapicBase = 0;

/**
 * @brief All cpu local data, indexed by cpu id.
 */
static lapic_t lapics[CPU_MAX] = {[0 ... CPU_MAX - 1] = {.lapicId = -1, .ticksPerMs = 0}};

/**
 * @brief Read from a local apic register.
 *
 * @param reg The register to read from.
 * @return The value read from the register.
 */
static uint32_t lapic_read(uint32_t reg)
{
    return READ_32(lapicBase + reg);
}

/**
 * @brief Write to a local apic register.
 *
 * @param reg The register to write to.
 * @param value The value to write.
 */
static void lapic_write(uint32_t reg, uint32_t value)
{
    WRITE_32(lapicBase + reg, value);
}

/**
 * @brief Initialize the local APIC for a CPU.
 *
 * @param cpu The current CPU.
 */
static void lapic_init(cpu_t* cpu)
{
    // Enable the local apic, enable spurious interrupts and mask everything for now.

    uint64_t lapicMsr = msr_read(MSR_LAPIC);
    msr_write(MSR_LAPIC, (lapicMsr | LAPIC_MSR_ENABLE) & ~LAPIC_MSR_BSP);

    lapic_write(LAPIC_REG_SPURIOUS, VECTOR_SPURIOUS | LAPIC_SPURIOUS_ENABLE);

    lapic_write(LAPIC_REG_LVT_TIMER, APIC_TIMER_MASKED);
    lapic_write(LAPIC_REG_LVT_ERROR, LAPIC_LVT_MASKED);
    lapic_write(LAPIC_REG_LVT_PERFCTR, LAPIC_LVT_MASKED);
    lapic_write(LAPIC_REG_LVT_THERMAL, LAPIC_LVT_MASKED);
    lapic_write(LAPIC_REG_LVT_LINT0, LAPIC_LVT_MASKED);
    lapic_write(LAPIC_REG_LVT_LINT1, LAPIC_LVT_MASKED);

    lapic_write(LAPIC_REG_TASK_PRIORITY, 0);

    lapics[cpu->id].lapicId = (lapic_id_t)(lapic_read(LAPIC_REG_ID) >> LAPIC_REG_ID_OFFSET);
}

/**
 * @brief Get the ticks per millisecond for the APIC timer of the current CPU.
 *
 * @return The ticks per millisecond.
 */
static uint64_t apic_timer_ticks_per_ms(void)
{
    interrupt_disable();

    lapic_write(LAPIC_REG_TIMER_DIVIDER, APIC_TIMER_DIV_DEFAULT);
    lapic_write(LAPIC_REG_LVT_TIMER, APIC_TIMER_MASKED);
    lapic_write(LAPIC_REG_TIMER_INITIAL_COUNT, UINT32_MAX);

    sys_time_wait(CLOCKS_PER_SEC / 1000);

    lapic_write(LAPIC_REG_LVT_TIMER, APIC_TIMER_MASKED);

    uint64_t ticks = UINT32_MAX - lapic_read(LAPIC_REG_TIMER_CURRENT_COUNT);
    lapic_write(LAPIC_REG_LVT_TIMER, APIC_TIMER_MASKED);
    lapic_write(LAPIC_REG_TIMER_INITIAL_COUNT, 0);

    interrupt_enable();
    return ticks;
}

/**
 * @brief Method for the APIC timer source to set the timer.
 */
static void apic_timer_set(irq_virt_t virt, clock_t uptime, clock_t timeout)
{
    (void)uptime;

    lapic_t* lapic = &lapics[cpu_get_id_unsafe()];
    if (lapic->ticksPerMs == 0)
    {
        lapic->ticksPerMs = apic_timer_ticks_per_ms();
    }

    lapic_write(LAPIC_REG_LVT_TIMER, APIC_TIMER_MASKED);
    lapic_write(LAPIC_REG_TIMER_INITIAL_COUNT, 0);

    if (timeout == CLOCKS_NEVER)
    {
        return;
    }

    uint32_t ticks = (timeout * lapic->ticksPerMs) / (CLOCKS_PER_SEC / 1000);
    if (ticks == 0)
    {
        ticks = 1;
    }

    lapic_write(LAPIC_REG_TIMER_DIVIDER, APIC_TIMER_DIV_DEFAULT);
    lapic_write(LAPIC_REG_LVT_TIMER, ((uint32_t)virt) | APIC_TIMER_ONE_SHOT);
    lapic_write(LAPIC_REG_TIMER_INITIAL_COUNT, ticks);
}

static void apic_timer_eoi(cpu_t* cpu)
{
    (void)cpu;

    lapic_write(LAPIC_REG_EOI, 0);
}

/**
 * @brief APIC timer source.
 *
 * According to https://telematics.tm.kit.edu/publications/Files/61/walter_ibm_linux_challenge.pdf, the APIC timer has a
 * precision of 1 microsecond.
 */
static timer_source_t apicTimer = {
    .name = "APIC Timer",
    .precision = 1000, // 1 microsecond
    .set = apic_timer_set,
    .ack = NULL,
    .eoi = apic_timer_eoi,
};

/**
 * @brief Read a value from an IO APIC register.
 *
 * @param ioapic Pointer to the IO APIC structure.
 * @param reg The register to read from.
 * @return The value read from the register.
 */
static uint32_t ioapic_read(ioapic_t* ioapic, ioapic_register_t reg)
{
    uintptr_t base = PML_LOWER_TO_HIGHER(ioapic->ioApicAddress);
    WRITE_32(base + IOAPIC_MMIO_REG_SELECT, reg);
    return READ_32(base + IOAPIC_MMIO_REG_DATA);
}

/**
 * @brief Write a value to an IO APIC register.
 *
 * @param ioapic Pointer to the IO APIC structure.
 * @param reg The register to write to.
 * @param value The value to write.
 */
static void ioapic_write(ioapic_t* ioapic, ioapic_register_t reg, uint32_t value)
{
    uintptr_t base = PML_LOWER_TO_HIGHER(ioapic->ioApicAddress);
    WRITE_32(base + IOAPIC_MMIO_REG_SELECT, reg);
    WRITE_32(base + IOAPIC_MMIO_REG_DATA, value);
}

/**
 * @brief Read the value in the `IOAPIC_REG_VERSION` register.
 *
 * This value is more complex than just a integer, so we use the `ioapic_version_t` structure to represent it.
 *
 * @param ioapic Pointer to the IO APIC structure.
 * @return The IO APIC version structure.
 */
static ioapic_version_t ioapic_version_read(ioapic_t* ioapic)
{
    ioapic_version_t version;
    version.raw = ioapic_read(ioapic, IOAPIC_REG_VERSION);
    return version;
}

/**
 * @brief Write a redirection entry to the IO APIC.
 *
 * @note The redirection entry is a total of 64 bits, but since the IO APIC registers are 32 bits wide, it ends up split
 * between two registers.
 *
 * @param ioapic Pointer to the IO APIC structure.
 * @param gsi The global system interrupt (physical IRQ) to write the redirection entry for.
 * @param entry The redirection entry to write.
 */
static void ioapic_redirect_write(ioapic_t* ioapic, ioapic_gsi_t gsi, ioapic_redirect_entry_t entry)
{
    assert(ioapic != NULL);
    assert(gsi >= ioapic->globalSystemInterruptBase);
    assert(gsi < ioapic->globalSystemInterruptBase + ioapic_version_read(ioapic).maxRedirs);

    uint32_t pin = gsi - ioapic->globalSystemInterruptBase;
    ioapic_write(ioapic, IOAPIC_REG_REDIRECTION_BASE + (pin * 2), entry.raw.low);
    ioapic_write(ioapic, IOAPIC_REG_REDIRECTION_BASE + (pin * 2) + 1, entry.raw.high);
}

/**
 * @brief Method to enable an IRQ in the IO APIC.
 */
static uint64_t ioapic_enable(irq_t* irq)
{
    ioapic_t* ioapic = irq->domain->private;
    lapic_t* lapic = &lapics[cpu_get_id_unsafe()];

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

/**
 * @brief Method to disable an IRQ in the IO APIC.
 */
static void ioapic_disable(irq_t* irq)
{
    ioapic_t* ioapic = irq->domain->private;

    ioapic_redirect_entry_t redirect = {.mask = 1};

    ioapic_redirect_write(ioapic, irq->phys, redirect);
}

/**
 * @brief Method to send an EOI in the IO APIC.
 */
static void ioapic_eoi(irq_t* irq)
{
    (void)irq;

    lapic_write(LAPIC_REG_EOI, 0);
}

/**
 * @brief IO APIC IRQ Chip.
 */
static irq_chip_t ioApicChip = {
    .name = "IO APIC",
    .enable = ioapic_enable,
    .disable = ioapic_disable,
    .ack = NULL,
    .eoi = ioapic_eoi,
};

/**
 * @brief Initialize all IO APICs found in the MADT.
 *
 * @return On success, `0`. On failure, `ERR`.
 */
static uint64_t ioapic_all_init(void)
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

/**
 * @brief Method to invoke an IPI using the local APIC.
 */
static void lapic_interrupt(cpu_t* cpu, irq_virt_t virt)
{
    lapic_id_t id = lapics[cpu->id].lapicId;

    lapic_write(LAPIC_REG_ICR1, id << LAPIC_REG_ID_OFFSET);
    lapic_write(LAPIC_REG_ICR0, (uint32_t)virt | LAPIC_ICR_FIXED);
}

/**
 * @brief Method to EOI an IPI.
 */
static void lapic_eoi(cpu_t* cpu)
{
    (void)cpu;

    lapic_write(LAPIC_REG_EOI, 0);
}

/**
 * @brief Local APIC IPI Chip.
 */
static ipi_chip_t lapicIpiChip = {
    .name = "Local APIC IPI",
    .interrupt = lapic_interrupt,
    .eoi = lapic_eoi,
};

/**
 * @brief Initialize the APIC global variables and state.
 *
 * @return On success, `0`. On failure, `ERR`.
 */
static uint64_t apic_init(void)
{
    madt_t* madt = (madt_t*)acpi_tables_lookup(MADT_SIGNATURE, sizeof(madt_t), 0);
    if (madt == NULL)
    {
        LOG_ERR("no MADT table found\n");
        return ERR;
    }

    if (madt->header.length < sizeof(madt_t))
    {
        LOG_ERR("madt table too small\n");
        return ERR;
    }

    if (madt->localInterruptControllerAddress == (uintptr_t)NULL)
    {
        LOG_ERR("madt has invalid lapic address\n");
        return ERR;
    }

    lapicBase = PML_LOWER_TO_HIGHER(madt->localInterruptControllerAddress);
    if (vmm_map(NULL, (void*)lapicBase, (void*)(uintptr_t)madt->localInterruptControllerAddress, PAGE_SIZE,
            PML_WRITE | PML_GLOBAL | PML_PRESENT, NULL, NULL) == NULL)
    {
        LOG_ERR("failed to map local apic\n");
        return ERR;
    }

    LOG_INFO("local apic mapped base=0x%016lx phys=0x%016lx\n", lapicBase,
        (uintptr_t)madt->localInterruptControllerAddress);

    if (timer_source_register(&apicTimer) == ERR)
    {
        vmm_unmap(NULL, (void*)lapicBase, PAGE_SIZE);
        LOG_ERR("failed to register apic timer source\n");
        return ERR;
    }

    if (ipi_chip_register(&lapicIpiChip) == ERR)
    {
        vmm_unmap(NULL, (void*)lapicBase, PAGE_SIZE);
        timer_source_unregister(&apicTimer);
        LOG_ERR("failed to register lapic ipi chip\n");
        return ERR;
    }

    return 0;
}

/*

void lapic_send_init(lapic_id_t id)
{
    if (!initialized)
    {
        panic(NULL, "local apic used before apic initialized");
    }

    lapic_write(LAPIC_REG_ICR1, id << LAPIC_REG_ID_OFFSET);
    lapic_write(LAPIC_REG_ICR0, LAPIC_ICR_INIT);
}

void lapic_send_sipi(lapic_id_t id, void* entryPoint)
{
    if (!initialized)
    {
        panic(NULL, "local apic used before apic initialized");
    }

    assert((uintptr_t)entryPoint % PAGE_SIZE == 0);

    lapic_write(LAPIC_REG_ICR1, id << LAPIC_REG_ID_OFFSET);
    lapic_write(LAPIC_REG_ICR0, LAPIC_ICR_STARTUP | ((uintptr_t)entryPoint / PAGE_SIZE));
}

*/

/** @} */

static void apic_cpu_handler(cpu_t* cpu, const cpu_event_t* event)
{
    (void)cpu;

    switch (event->type)
    {
    case CPU_ONLINE:
    {
        lapic_init(cpu);
    }
    break;
    default:
        break;
    }
}

uint64_t _module_procedure(const module_event_t* event)
{
    switch (event->type)
    {
    case MODULE_EVENT_DEVICE_ATTACH:
        if (apic_init() == ERR)
        {
            LOG_ERR("failed to initialize apic\n");
            return ERR;
        }
        if (ioapic_all_init() == ERR)
        {
            LOG_ERR("failed to initialize ioapics\n");
            return ERR;
        }
        if (cpu_handler_register(apic_cpu_handler) == ERR)
        {
            LOG_ERR("failed to register apic cpu event handler\n");
            return ERR;
        }
        break;
    default:
        break;
    }

    return 0;
}

MODULE_INFO("APIC Driver", "Kai Norberg", "A driver for the APIC, local APIC and IOAPIC", OS_VERSION, "MIT", "PNP0003");