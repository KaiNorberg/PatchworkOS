#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/io.h>
#include <sys/proc.h>
#include <threads.h>

#define PRIME_MAX (1 << 17)

static atomic_long count;

static uint64_t threadAmount;

static bool is_prime(uint64_t num)
{
    if (num == 0 || num == 1)
    {
        return false;
    }

    for (uint64_t i = 2; i <= num / 2; i++)
    {
        if (num % i == 0)
        {
            return false;
        }
    }

    return true;
}

static void count_primes(uint64_t low, uint64_t high)
{
    for (uint64_t i = low; i < high; i++)
    {
        if (is_prime(i))
        {
            atomic_fetch_add(&count, 1);
        }
    }
}

static int thread_entry(void* arg)
{
    uint64_t index = (uint64_t)arg;

    uint64_t delta = PRIME_MAX / threadAmount;
    uint64_t low = delta * index;
    uint64_t high = low + delta;

    count_primes(low, high);
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

static void benchmark(void)
{
    printnum(threadAmount);
    print(" threads: ");

    nsec_t start = uptime();
    print("starting... ");

    atomic_init(&count, 0);

    thrd_t threads[threadAmount];
    for (uint64_t i = 0; i < threadAmount; i++)
    {
        thrd_create(&threads[i], thread_entry, (void*)i);
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
    threadAmount = 1;
    benchmark();
    threadAmount = 2;
    benchmark();
    threadAmount = 4;
    benchmark();

    return 0;
}
