#include <kernel/drivers/hpet.h>

#include <kernel/acpi/tables.h>
#include <kernel/cpu/smp.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/mem/vmm.h>
#include <kernel/sched/timer.h>
#include <kernel/sync/seqlock.h>
#include <kernel/utils/utils.h>

#include <assert.h>

static hpet_t* hpet;
static uintptr_t address;
static uint64_t period; // Main counter tick period in femtoseconds (10^-15 s).

static bool isInitialized = false;

static atomic_uint64_t counter = ATOMIC_VAR_INIT(0);
static seqlock_t counterLock = SEQLOCK_CREATE;

static inline void hpet_write(uint64_t reg, uint64_t value)
{
    WRITE_64(address + reg, value);
}

static inline uint64_t hpet_read(uint64_t reg)
{
    return READ_64(address + reg);
}

static inline clock_t hpet_ns_per_tick(void)
{
    return period / (HPET_FEMTOSECONDS_PER_SECOND / CLOCKS_PER_SEC);
}

static inline uint64_t hpet_read_counter(void)
{
    return hpet_read(HPET_REG_MAIN_COUNTER_VALUE);
}

static inline void hpet_reset_counter(void)
{
    if (!isInitialized)
    {
        return;
    }
    hpet_write(HPET_REG_GENERAL_CONFIG, 0);
    hpet_write(HPET_REG_MAIN_COUNTER_VALUE, 0);
    hpet_write(HPET_REG_GENERAL_CONFIG, HPET_CONF_ENABLE_CNF_BIT);
}

static inline clock_t hpet_pesimistic_overflow_interval(void)
{
    uint64_t maxCounterValue = UINT32_MAX;
    return (maxCounterValue * hpet_ns_per_tick()) / 2;
}

static void hpet_timer_handler(interrupt_frame_t* frame, cpu_t* self)
{
    (void)frame;

    LOG_INFO("HPET counter accumulation timer fired\n");
    seqlock_write_acquire(&counterLock);
    atomic_fetch_add(&counter, hpet_read_counter() * hpet_ns_per_tick());
    hpet_reset_counter();
    seqlock_write_release(&counterLock);

    timer_one_shot(self, hpet_pesimistic_overflow_interval(), timer_uptime());
}

static uint64_t hpet_init(sdt_header_t* table)
{
    hpet = (hpet_t*)table;

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

    isInitialized = true;

    uint64_t capabilities = hpet_read(HPET_REG_GENERAL_CAPABILITIES_ID);
    period = capabilities >> HPET_CAP_COUNTER_CLK_PERIOD_SHIFT;

    if (period == 0 || period >= 0x05F5E100)
    {
        LOG_ERR("HPET reported an invalid counter period %llu fs\n", period);
        isInitialized = false;
        return ERR;
    }

    LOG_INFO("started HPET timer phys=0x%016lx virt=0x%016lx period=%lluns timers=%u %s-bit\n", hpet->address, address,
        period / (HPET_FEMTOSECONDS_PER_SECOND / CLOCKS_PER_SEC), hpet->comparatorCount + 1,
        hpet->counterIs64Bit ? "64" : "32");

    hpet_reset_counter();

    clock_t overflowInterval = hpet_pesimistic_overflow_interval();
    LOG_INFO("scheduling HPET counter accumulation timer every %llums\n", overflowInterval / (CLOCKS_PER_SEC / 1000));
    timer_one_shot(smp_self_unsafe(), overflowInterval, timer_uptime());
    return 0;
}

ACPI_SDT_HANDLER_REGISTER("HPET", hpet_init);

clock_t hpet_read_ns_counter(void)
{
    if (!isInitialized)
    {
        return 0;
    }
    clock_t time;
    uint64_t seq;
    do
    {
        seq = seqlock_read_begin(&counterLock);
        time = atomic_load(&counter) + hpet_read_counter() * hpet_ns_per_tick();
    } while (seqlock_read_retry(&counterLock, seq));
    return time;
}

void hpet_wait(clock_t nanoseconds)
{
    if (!isInitialized)
    {
        panic(NULL, "HPET not initialized");
    }

    if (nanoseconds == 0)
    {
        return;
    }

    uint64_t ticks = (nanoseconds * 1000000) / period;
    uint64_t start = hpet_read_counter();
    while (hpet_read_counter() < start + ticks)
    {
        asm volatile("pause");
    }
}
