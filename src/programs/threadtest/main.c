#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/proc.h>
#include <threads.h>

#undef _USE_ANNEX_K
#define _USE_ANNEX_K 1
#include "../../libstd/platform/user/common/syscalls.h"

#define PRIME_MAX (10000000)

static atomic_long count;
static atomic_long next;

bool is_prime(uint64_t n)
{
    if (n <= 1)
    {
        return false;
    }
    if (n <= 3)
    {
        return true;
    }
    if (n % 2 == 0 || n % 3 == 0)
    {
        return false;
    }

    for (uint64_t i = 5; i * i <= n; i += 6)
    {
        if (n % i == 0 || n % (i + 2) == 0)
        {
            return false;
        }
    }

    return true;
}

static void count_primes(uint64_t start, uint64_t end)
{
    for (uint64_t i = start; i < end; i++)
    {
        if (is_prime(i))
        {
            atomic_fetch_add(&count, 1);
        }
    }
}

static int thread_entry(void* arg)
{
    (void)arg;

    uint64_t start;
    while ((start = atomic_fetch_add(&next, 1000)) < PRIME_MAX)
    {
        uint64_t end = start + 1000;
        if (end > PRIME_MAX)
        {
            end = PRIME_MAX;
        }

        count_primes(start, end);
    }
    return thrd_success;
}

static void benchmark(uint64_t threadAmount)
{
    printf("%d threads: starting...", threadAmount);
    fflush(stdout);
    clock_t start = uptime();

    atomic_init(&count, 0);
    atomic_init(&next, 0);

    thrd_t threads[threadAmount];
    for (uint64_t i = 0; i < threadAmount; i++)
    {
        if (thrd_create(&threads[i], thread_entry, NULL) != thrd_success)
        {
            printf(" (thrd_create error %d) ", i);
            fflush(stdout);
        }
    }

    for (uint64_t i = 0; i < threadAmount; i++)
    {
        if (thrd_join(threads[i], NULL) != thrd_success)
        {
            printf(" (thrd_join error %d) ", i);
            fflush(stdout);
        }
    }

    clock_t end = uptime();
    printf(" took %d ms to find %d primes\n", (end - start) / (CLOCKS_PER_SEC / 1000), atomic_load(&count));
}

int main(void)
{
    for (uint64_t threads = 1; threads <= 1024; threads *= 2)
    {
        benchmark(threads);
    }

    printf("Testing complete.\n");
    return 0;
}
