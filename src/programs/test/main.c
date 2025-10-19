#include <stdint.h>
#include <sys/proc.h>
#include <sys/io.h>
#include <stdio.h>
#include <time.h>

#define TEST_ITERATIONS 100000

#define TEST_MAX_PAGES (1 << 16)

static fd_t zeroDev;

static void benchmark_mmap(uint64_t size)
{
    clock_t start = clock();

    for (uint64_t i = 0; i < TEST_ITERATIONS; i++)
    {
        void* ptr = mmap(zeroDev, NULL, size * 0x1000, PROT_READ | PROT_WRITE);

        for (uint64_t j = 0; j < size; j++)
        {
            ((uint8_t*)ptr)[j] = 0;
        }

        munmap(ptr, size);
    }

    clock_t end = clock();
    printf("mmap size=%llu bytes: %llums\n", size, (end - start) / (CLOCKS_PER_SEC / 1000));
}

int main()
{
    zeroDev = open("/dev/zero");

    printf("Starting mmap benchmark with %llu iterations\n", TEST_ITERATIONS);
    for (uint64_t i = 1; i <= TEST_MAX_PAGES; i *= 2)
    {
        benchmark_mmap(i);
    }

    return 0;
}
