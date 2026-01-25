#include <kernel/drivers/rand.h>

#include <kernel/cpu/cpu.h>
#include <kernel/log/log.h>
#include <kernel/sched/clock.h>

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/cpuid.h>

static atomic_uint64_t seed = ATOMIC_VAR_INIT(0x123456789ABCDEF0);

static uint64_t rand_gen_fallback(void* buffer, uint64_t size)
{
    for (uint64_t i = 0; i < size; i++)
    {
        uint64_t currentSeed = atomic_load(&seed) + clock_uptime();
        currentSeed ^= currentSeed << 13;
        currentSeed ^= currentSeed >> 7;
        currentSeed ^= currentSeed << 17;
        atomic_store(&seed, currentSeed);
        ((uint8_t*)buffer)[i] = (uint8_t)(currentSeed & 0xFF);
    }

    return 0;
}

PERCPU_DEFINE_CTOR(rand_cpu_t, pcpu_rand)
{
    rand_cpu_t* ctx = SELF_PTR(pcpu_rand);

    cpuid_feature_info_t info;
    cpuid_feature_info(&info);

    ctx->rdrandAvail = (info.featuresEcx & CPUID_ECX_RDRAND) != 0;

    if (!ctx->rdrandAvail)
    {
        return;
    }

    uint64_t prev = UINT64_MAX;
    uint32_t test;
    for (uint64_t i = 0; i < 10; i++)
    {
        if (rdrand_do(&test, 100) == _FAIL)
        {
            LOG_WARN("cpu%d rdrand instruction failed, disabling\n", SELF->id);
            ctx->rdrandAvail = false;
            return;
        }

        if (prev != UINT64_MAX && prev == test)
        {
            LOG_WARN("cpu%d rdrand producing same value repeatedly, disabling\n", SELF->id);
            ctx->rdrandAvail = false;
            return;
        }
        prev = test;
    }
}

uint64_t rand_gen(void* buffer, uint64_t size)
{
    CLI_SCOPE();

    if (!pcpu_rand->rdrandAvail)
    {
        return rand_gen_fallback(buffer, size);
    }

    uint8_t* ptr = (uint8_t*)buffer;
    uint64_t remaining = size;
    while (remaining >= sizeof(uint32_t))
    {
        uint32_t value;
        if (rdrand_do(&value, 100) == _FAIL)
        {
            return _FAIL;
        }
        memcpy(ptr, &value, sizeof(uint32_t));
        ptr += sizeof(uint32_t);
        remaining -= sizeof(uint32_t);
    }

    if (remaining > 0)
    {
        uint32_t value;
        if (rdrand_do(&value, 100) == _FAIL)
        {
            return _FAIL;
        }
        memcpy(ptr, &value, remaining);
    }

    return 0;
}
