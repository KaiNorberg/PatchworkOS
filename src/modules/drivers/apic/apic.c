#include <kernel/acpi/tables.h>
#include <kernel/cpu/cpu.h>
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
 * @ingroup kernel_drivers
 * @defgroup kernel_drivers_apic APIC
 *
 * @see [ACPI Specification Version 6.6](https://uefi.org/sites/default/files/resources/ACPI_Spec_6.6.pdf)
 *
 * @{
 */

/**
 * @brief Local APIC ID type.
 */
typedef uint8_t lapic_id_t;

/**
 * @brief IO APIC Global System Interrupt type.
 */
typedef uint32_t ioapic_gsi_t;

/**
 * @brief APIC Timer Modes.
 * @enum apic_timer_mode_t
 */
typedef enum
{
    APIC_TIMER_MASKED = 0x10000, ///< Timer is masked (disabled)
    APIC_TIMER_PERIODIC = 0x20000,
    APIC_timer_set = 0x00000
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
#define LAPIC_REG_ICR1_ID_OFFSET 24

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
    IOAPIC_REG_VERSION = 0x01
} ioapic_register_t;

/**
 * @brief Macro to get the redirection entry register for a specific pin.
 *
 * This is used since a redirect entry is 64 bits (a qword/two dwords) and each register is 32 bits (a dword), so each
 * pin uses two registers.
 *
 * @param pin The pin number as in the gsi - the ioapics base gsi.
 * @param high 0 for the low dword, 1 for the high dword.
 * @return The register number.
 */
#define IOAPIC_REG_REDIRECTION(pin, high) (0x10 + (pin) * 2 + (high))

/**
 * @brief APIC Timer Ticks Fixed Point Offset.
 *
 * Used for fixed point arithmetic when returning the apic timer ticks per nanosecond.
 */
#define APIC_TIMER_TICKS_FIXED_POINT_OFFSET 32

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
 * @brief IO APIC Redirection Entry Structure.
 * @struct ioapic_redirect_entry_t
 *
 * Represents a single redirection entry in the IO APIC.
 */
typedef union {
    struct PACKED
    {
        uint8_t vector;
        uint8_t deliveryMode : 3;
        uint8_t destinationMode : 1;
        uint8_t deliveryStatus : 1;
        uint8_t polarity : 1;
        uint8_t remoteIRR : 1;
        uint8_t triggerMode : 1;
        uint8_t mask : 1;
        uint64_t reserved : 39;
        uint8_t destination : 8;
    };
    struct PACKED
    {
        uint32_t low;
        uint32_t high;
    } raw;
} ioapic_redirect_entry_t;

static uintptr_t lapicBase;

static uint64_t lapic_init(void)
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
    return 0;
}

/*static uint64_t ioapic_all_init(void)
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

void apic_timer_set(interrupt_t vector, uint32_t ticks)
{
    if (!initialized)
    {
        panic(NULL, "apic timer used before apic initialized");
    }

    lapic_write(LAPIC_REG_LVT_TIMER, APIC_TIMER_MASKED);

    lapic_write(LAPIC_REG_TIMER_DIVIDER, APIC_TIMER_DIV_DEFAULT);
    lapic_write(LAPIC_REG_LVT_TIMER, ((uint32_t)vector) | APIC_timer_set);
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

    sys_time_wait(CLOCKS_PER_SEC / 1000);

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
}

lapic_id_t lapic_get_id(void)
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

void lapic_send_ipi(lapic_id_t id, interrupt_t vector)
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

void ioapic_set_redirect(interrupt_t vector, ioapic_gsi_t gsi, ioapic_delivery_mode_t deliveryMode,
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
}*/

/** @} */

uint64_t _module_procedure(const module_event_t* event)
{
    switch (event->type)
    {
    case MODULE_EVENT_DEVICE_ATTACH:
        if (lapic_init() == ERR)
        {
            LOG_ERR("failed to initialize apic\n");
            return ERR;
        }
        return 0;
    case MODULE_EVENT_DEVICE_DETACH:
        return 0;
    default:
        return ERR;
    }
}

MODULE_INFO("APIC Driver", "Kai Norberg", "A driver for the APIC, local APIC and IOAPIC", OS_VERSION, "MIT", "PNP0003");