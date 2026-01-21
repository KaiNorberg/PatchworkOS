#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define MMAP_ITER 10000
#define GETPID_ITER 100000

#ifdef _PATCHWORK_OS_
#include <sys/fs.h>
#include <sys/proc.h>

static fd_t zeroDev;

static void init_generic()
{
    zeroDev = open("/dev/const/zero");
    if (zeroDev == ERR)
    {
        perror("Failed to open /dev/const/zero");
        abort();
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
    return munmap(addr, length) == NULL ? ERR : 0;
}

static void benchmark_getpid(void)
{
    clock_t start = clock();

    for (uint64_t i = 0; i < GETPID_ITER; i++)
    {
        getpid();
    }

    clock_t end = clock();
    printf("getpid: %llums\n", (end - start) / (CLOCKS_PER_MS));

    clock_t procStart = clock();

    char buffer[32];
    for (uint64_t i = 0; i < GETPID_ITER; i++)
    {
        readfile("/proc/self/pid", buffer, sizeof(buffer), 0);
    }

    clock_t procEnd = clock();
    printf("/proc/self/pid: %llums\n", (procEnd - procStart) / (CLOCKS_PER_MS));

    printf("overhead: %lluns\n", ((procEnd - procStart) - (end - start)) / GETPID_ITER);
}

#else

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#define ERR ((uint64_t)-1)

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

    for (uint64_t i = 0; i < MMAP_ITER; i++)
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
    printf("mmap pages=%llu bytes: %llums\n", pages, (end - start) / (CLOCKS_PER_MS));
}

int main()
{
    init_generic();

#ifdef _PATCHWORK_OS_
    benchmark_getpid();
#endif

    benchmark_mmap(1);
    for (uint64_t i = 50; i <= 1500; i += 50)
    {
        benchmark_mmap(i);
    }

    return 0;
}
