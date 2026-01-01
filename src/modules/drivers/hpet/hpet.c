#include <kernel/cpu/cpu.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/mem/vmm.h>
#include <kernel/module/module.h>
#include <kernel/sched/clock.h>
#include <kernel/sched/sched.h>
#include <kernel/sched/thread.h>
#include <kernel/sched/wait.h>
#include <kernel/sync/seqlock.h>
#include <kernel/utils/utils.h>
#include <modules/acpi/tables.h>

/**
 * @brief High Precision Event Timer
 * @defgroup modules_drivers_hpet HPET
 * @ingroup modules_drivers
 *
 * @note Since the HPET might be 32bit it could overflow rather quickly, so we implement a system for checking roughly
 * when it will overflow and accumulate the counter into a 64 bit nanosecond counter.
 *
 * @see [OSDev HPET](https://wiki.osdev.org/HPET)
 *
 * @{
 */

/**
 * @brief HPET register offsets
 * @enum hpet_register_t
 */
typedef enum
{
    HPET_REG_GENERAL_CAPABILITIES_ID = 0x000,
    HPET_REG_GENERAL_CONFIG = 0x010,
    HPET_REG_GENERAL_INTERRUPT = 0x020,
    HPET_REG_MAIN_COUNTER_VALUE = 0x0F0,
    HPET_REG_TIMER0_CONFIG_CAP = 0x100,
    HPET_REG_TIMER0_COMPARATOR = 0x108,
} hpet_register_t;

/**
 * @brief The bit offset of the clock period in the capabilities register
 */
#define HPET_CAP_COUNTER_CLK_PERIOD_SHIFT 32

/**
 * @brief The bit to set to enable the HPET in the configuration register
 */
#define HPET_CONF_ENABLE_CNF_BIT (1 << 0)

/**
 * @brief The bit to set to enable legacy replacement mode in the configuration register
 */
#define HPET_CONF_LEG_RT_CNF_BIT (1 << 1)

/**
 * @brief If `hpet_t::addressSpaceId` is equal to this, the address is in system memory space.
 */
#define HPET_ADDRESS_SPACE_MEMORY 0

/**
 * @brief If `hpet_t::addressSpaceId` is equal to this, the address is in system I/O space.
 */
#define HPET_ADDRESS_SPACE_IO 1

/**
 * @brief The number of femtoseconds in one second
 */
#define HPET_FEMTOSECONDS_PER_SECOND 1000000000000000ULL

/**
 * @brief High Precision Event Timer structure
 * @struct hpet_t
 */
typedef struct PACKED
{
    sdt_header_t header;
    uint8_t hardwareRevId;
    uint8_t comparatorCount : 5;
    uint8_t counterIs64Bit : 1;
    uint8_t reserved1 : 1;
    uint8_t legacyReplacementCapable : 1;
    uint16_t pciVendorId;
    uint8_t addressSpaceId;
    uint8_t registerBitWidth;
    uint8_t registerBitOffset;
    uint8_t reserved2;
    uint64_t address;
    uint8_t hpetNumber;
    uint16_t minimumTick;
    uint8_t pageProtection;
} hpet_t;

static hpet_t* hpet;      ///< Pointer to the HPET ACPI table.
static uintptr_t address; ///< Mapped virtual address of the HPET registers.
static uint64_t period;   ///< Main counter tick period in femtoseconds (10^-15 s).

static atomic_uint64_t counter = ATOMIC_VAR_INIT(0); ///< Accumulated nanosecond counter, used to avoid overflows.
static seqlock_t counterLock = SEQLOCK_CREATE();     ///< Seqlock for the accumulated counter.

static tid_t overflowThreadTid = 0;                                   ///< Thread ID of the overflow thread.
static wait_queue_t overflowQueue = WAIT_QUEUE_CREATE(overflowQueue); ///< Wait queue for the overflow thread.
static atomic_bool overflowShouldStop = ATOMIC_VAR_INIT(false);       ///< Flag to signal the overflow thread to stop.

/**
 * @brief Write to an HPET register.
 *
 * @param reg The register to write to.
 * @param value The value to write.
 */
static inline void hpet_write(hpet_register_t reg, uint64_t value)
{
    WRITE_64(address + reg, value);
}

/**
 * @brief Read from an HPET register.
 *
 * @param reg The register to read from.
 * @return The value read from the register.
 */
static inline uint64_t hpet_read(hpet_register_t reg)
{
    return READ_64(address + reg);
}

/**
 * @brief Get the HPET clock period in nanoseconds.
 *
 * @return The HPET clock period in nanoseconds.
 */
static inline clock_t hpet_ns_per_tick(void)
{
    return period / (HPET_FEMTOSECONDS_PER_SECOND / CLOCKS_PER_SEC);
}

/**
 * @brief Safely read the HPET counter value in nanoseconds.
 *
 * @return The current value of the HPET counter in nanoseconds.
 */
static clock_t hpet_read_ns_counter(void)
{
    clock_t time;
    uint64_t seq;
    do
    {
        seq = seqlock_read_begin(&counterLock);
        time = atomic_load(&counter) + hpet_read(HPET_REG_MAIN_COUNTER_VALUE) * hpet_ns_per_tick();
    } while (seqlock_read_retry(&counterLock, seq));
    return time;
}

/**
 * @brief Reset the HPET main counter to zero and enable the HPET.
 */
static inline void hpet_reset_counter(void)
{
    hpet_write(HPET_REG_GENERAL_CONFIG, 0);
    hpet_write(HPET_REG_MAIN_COUNTER_VALUE, 0);
    hpet_write(HPET_REG_GENERAL_CONFIG, HPET_CONF_ENABLE_CNF_BIT);
}

/**
 * @brief Thread function that periodically accumulates the HPET counter to prevent overflow.
 *
 * @param arg Unused.
 */
static void hpet_overflow_thread(void* arg)
{
    UNUSED(arg);

    // Assume the worst case where the HPET is 32bit, since `clock_t` isent large enough to hold the time otherwise and
    // i feel paranoid.
    clock_t sleepInterval = (UINT32_MAX * hpet_ns_per_tick()) / 2;
    LOG_INFO("HPET overflow thread started, sleep interval %lluns\n", sleepInterval);

    while (!atomic_load(&overflowShouldStop))
    {
        WAIT_BLOCK_TIMEOUT(&overflowQueue, false, sleepInterval);

        seqlock_write_acquire(&counterLock);
        atomic_fetch_add(&counter, hpet_read(HPET_REG_MAIN_COUNTER_VALUE) * hpet_ns_per_tick());
        hpet_reset_counter();
        seqlock_write_release(&counterLock);
    }
}

/**
 * @brief Structure to describe the HPET to the sys time subsystem.
 */
static clock_source_t source = {
    .name = "HPET",
    .precision = 0, // Filled in during init
    .read_ns = hpet_read_ns_counter,
    .read_epoch = NULL,
};

/**
 * @brief Initialize the HPET.
 *
 * @return On success, `0`. On failure, `ERR`.
 */
static uint64_t hpet_init(void)
{
    hpet = (hpet_t*)acpi_tables_lookup("HPET", sizeof(hpet_t), 0);
    if (hpet == NULL)
    {
        LOG_ERR("failed to locate HPET table\n");
        return ERR;
    }

    if (hpet->addressSpaceId != HPET_ADDRESS_SPACE_MEMORY)
    {
        LOG_ERR("HPET address space is not memory (id=%u) which is not supported\n", hpet->addressSpaceId);
        return ERR;
    }

    address = (uintptr_t)PML_LOWER_TO_HIGHER(hpet->address);
    if (vmm_map(NULL, (void*)address, (void*)hpet->address, PAGE_SIZE, PML_WRITE | PML_GLOBAL | PML_PRESENT, NULL,
            NULL) == NULL)
    {
        LOG_ERR("failed to map HPET memory at 0x%016lx\n", hpet->address);
        return ERR;
    }

    uint64_t capabilities = hpet_read(HPET_REG_GENERAL_CAPABILITIES_ID);
    period = capabilities >> HPET_CAP_COUNTER_CLK_PERIOD_SHIFT;

    if (period == 0 || period >= 0x05F5E100)
    {
        LOG_ERR("HPET reported an invalid counter period %llu fs\n", period);
        return ERR;
    }

    LOG_INFO("started HPET timer phys=0x%016lx virt=0x%016lx period=%lluns timers=%u %s-bit\n", hpet->address, address,
        period / (HPET_FEMTOSECONDS_PER_SECOND / CLOCKS_PER_SEC), hpet->comparatorCount + 1,
        hpet->counterIs64Bit ? "64" : "32");

    hpet_reset_counter();

    source.precision = hpet_ns_per_tick();
    if (clock_source_register(&source) == ERR)
    {
        LOG_ERR("failed to register HPET as system time source\n");
        return ERR;
    }

    overflowThreadTid = thread_kernel_create(hpet_overflow_thread, NULL);
    if (overflowThreadTid == ERR)
    {
        LOG_ERR("failed to create HPET overflow thread\n");
        clock_source_unregister(&source);
        return ERR;
    }

    return 0;
}

/**
 * @brief Deinitialize the HPET.
 */
static void hpet_deinit(void)
{
    atomic_store(&overflowShouldStop, true);
    wait_unblock(&overflowQueue, WAIT_ALL, EOK);

    while (process_has_thread(process_get_kernel(), overflowThreadTid))
    {
        sched_yield();
    }

    clock_source_unregister(&source);
    hpet_write(HPET_REG_GENERAL_CONFIG, 0);
}

/** @} */

uint64_t _module_procedure(const module_event_t* event)
{
    switch (event->type)
    {
    case MODULE_EVENT_DEVICE_ATTACH:
        if (hpet_init() == ERR)
        {
            panic(NULL, "Failed to initialize HPET module");
        }
        break;
    case MODULE_EVENT_DEVICE_DETACH:
        hpet_deinit();
        break;
    default:
        break;
    }

    return 0;
}

MODULE_INFO("HPET Driver", "Kai Norberg", "A High Precision Event Timer driver", OS_VERSION, "MIT", "PNP0103");
