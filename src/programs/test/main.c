#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/proc.h>
#include <time.h>

#define TEST_ITERATIONS 100000

#define TEST_MAX_SIZE 100000
#define TEST_STEP_SIZE (TEST_MAX_SIZE / 10)

uint8_t src[TEST_MAX_SIZE];
uint8_t dst[TEST_MAX_SIZE];

void benchmark_memcpy(uint64_t size)
{
    memset(src, 0xAA, size);

    clock_t start = uptime();
    for (int i = 0; i < TEST_ITERATIONS; i++)
    {
        memcpy(dst, src, size);
    }
    clock_t end = uptime();

    uint64_t elapsed_ms = (uint64_t)((end - start) * 1000 / CLOCKS_PER_SEC);
    printf("copy %zu bytes: %llu ms\n", size, elapsed_ms);
}

int main()
{
    printf("memcpy Benchmark Results:\n");
    printf("------------------------\n");

    for (uint64_t i = TEST_STEP_SIZE; i <= TEST_MAX_SIZE; i += TEST_STEP_SIZE)
    {
        benchmark_memcpy(i);
    }

    return 0;
}