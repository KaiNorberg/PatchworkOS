#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define TEST_ITERATIONS 10000

#ifdef __PATCHWORK_OS__
#include <sys/io.h>
#include <sys/proc.h>

static fd_t zeroDev;

static void init_generic()
{
    zeroDev = open("/dev/zero");
    if (zeroDev == ERR)
    {
        perror("Failed to open /dev/zero");
        exit(EXIT_FAILURE);
    }
}

static void* mmap_generic(size_t length)
{
    void* ptr = mmap(zeroDev, NULL, length, PROT_READ | PROT_WRITE);
    if (ptr == NULL)
    {
        return NULL;
    }
    return ptr;
}

static uint64_t munmap_generic(void* addr, size_t length)
{
    return munmap(addr, length);
}

#else

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

static void init_generic()
{
    // Nothing to do
}

static void* mmap_generic(size_t length)
{
    void* ptr = mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptr == MAP_FAILED)
    {
        return NULL;
    }
    return ptr;
}

static uint64_t munmap_generic(void* addr, size_t length)
{
    return munmap(addr, length) == -1 ? ERR : 0;
}

#endif

static void benchmark_mmap(uint64_t pages)
{
    clock_t start = clock();

    for (uint64_t i = 0; i < TEST_ITERATIONS; i++)
    {
        void* ptr = mmap_generic(pages * 0x1000);
        if (ptr == NULL)
        {
            perror("mmap failed");
            return;
        }

        for (uint64_t j = 0; j < pages; j++)
        {
            ((uint8_t*)ptr)[j * 0x1000] = 0;
        }

        if (munmap_generic(ptr, pages * 0x1000) != 0)
        {
            perror("munmap failed");
            return;
        }
    }

    clock_t end = clock();
    printf("mmap pages=%llu bytes: %llums\n", pages, (end - start) / (CLOCKS_PER_SEC / 1000));
}

int main()
{
    init_generic();

    printf("Starting mmap benchmark with %llu iterations\n", TEST_ITERATIONS);
    for (uint64_t i = 1; i <= 1024; i *= 2)
    {
        benchmark_mmap(i);
    }

    return 0;
}
