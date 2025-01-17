#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/io.h>
#include <sys/proc.h>
#include <threads.h>

#define PRIME_MAX (10000000)

static atomic_long count;
static atomic_long next;

bool is_prime(int n)
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

    for (int i = 5; i * i <= n; i += 6)
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
            atomic_fetch_add_explicit(&count, 1, __ATOMIC_RELAXED);
        }
    }
}

static int thread_entry(void* arg)
{
    uint64_t start;
    while ((start = atomic_fetch_add_explicit(&next, 1000, __ATOMIC_RELAXED)) < PRIME_MAX)
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

// Temporary becouse printf does not exist yet
static void print(const char* str)
{
    write(STDOUT_FILENO, str, strlen(str));
}

static void printnum(int num)
{
    char buffer[32];
    ulltoa(num, buffer, 10);
    print(buffer);
}

static void benchmark(uint64_t threadAmount)
{
    printnum(threadAmount);
    print(" threads: ");

    nsec_t start = uptime();
    print("starting... ");

    atomic_init(&count, 0);
    atomic_init(&next, 0);

    thrd_t threads[threadAmount];
    for (uint64_t i = 0; i < threadAmount; i++)
    {
        thrd_create(&threads[i], thread_entry, NULL);
    }

    for (uint64_t i = 0; i < threadAmount; i++)
    {
        thrd_join(threads[i], NULL);
    }

    print("took ");

    nsec_t end = uptime();
    printnum((end - start) / (SEC / 1000));
    print(" ms to find ");

    printnum(atomic_load(&count));
    print(" primes\n");
}

int main(void)
{
    benchmark(1);
    benchmark(2);
    benchmark(4);
    benchmark(8);

    return 0;
}
