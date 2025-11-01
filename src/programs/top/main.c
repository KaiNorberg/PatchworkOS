#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/io.h>
#include <sys/proc.h>
#include <threads.h>
#include <time.h>

#define SAMPLE_INTERVAL (CLOCKS_PER_SEC)

#define PLOT_WIDTH 20

static uint64_t terminalColumns;

static uint64_t terminal_columns_get(void)
{
    uint32_t terminalWidth = 80;
    printf("\033[999C\033[6n");
    fflush(stdout);
    char buffer[MAX_NAME] = {0};
    for (uint32_t i = 0; i < sizeof(buffer) - 1; i++)
    {
        read(STDIN_FILENO, &buffer[i], 1);
        if (buffer[i] == 'R')
        {
            break;
        }
    }
    int row;
    int cols;
    sscanf(buffer, "\033[%d;%dR", &row, &cols);

    if (cols != 0)
    {
        terminalWidth = (uint32_t)cols;
    }

    printf("\r");
    fflush(stdout);
    return terminalWidth;
}

typedef struct
{
    uint64_t id;
    clock_t idleClocks;
    clock_t activeClocks;
    clock_t interruptClocks;
} cpu_perfs_t;

typedef struct
{
    uint64_t totalKiB;
    uint64_t freeKiB;
    uint64_t reservedKiB;
} mem_perfs_t;

typedef struct
{
    uint64_t cpuAmount;
    cpu_perfs_t* prevCpuperfs;
    cpu_perfs_t* cpuperfs;
    mem_perfs_t memperfs;
} perfs_t;

static uint64_t cpu_perf_count_cpus(void)
{
    FILE* file = fopen("/dev/perf/cpu", "r");
    if (file == NULL)
    {
        return ERR;
    }

    uint64_t cpuCount = 0;
    char line[256];
    while (fgets(line, sizeof(line), file) != NULL)
    {
        cpuCount++;
    }

    fclose(file);
    return cpuCount - 1; // -1 due to header
}

static uint64_t cpu_perf_read(cpu_perfs_t* cpuperfs, uint64_t cpuAmount)
{
    FILE* file = fopen("/dev/perf/cpu", "r");
    if (file == NULL)
    {
        return ERR;
    }

    char line[256];
    fgets(line, sizeof(line), file); // Skip header line

    for (uint64_t i = 0; i < cpuAmount; i++)
    {
        if (fgets(line, sizeof(line), file) == NULL)
        {
            break;
        }

        if (sscanf(line, "cpu%d %llu %llu %llu", &cpuperfs[i].id, &cpuperfs[i].idleClocks, &cpuperfs[i].activeClocks,
                &cpuperfs[i].interruptClocks) != 4)
        {
            cpuperfs[i].id = 0;
            cpuperfs[i].idleClocks = 0;
            cpuperfs[i].activeClocks = 0;
            cpuperfs[i].interruptClocks = 0;
        }
    }

    fclose(file);
    return 0;
}

static uint64_t mem_perf_read(mem_perfs_t* memperfs)
{
    FILE* file = fopen("/dev/perf/mem", "r");
    if (file == NULL)
    {
        return ERR;
    }

    if (fscanf(file, "value kib\ntotal %llu\nfree %llu\nreserved %llu", &memperfs->totalKiB, &memperfs->freeKiB,
            &memperfs->reservedKiB) != 3)
    {
        memperfs->totalKiB = 0;
        memperfs->freeKiB = 0;
        memperfs->reservedKiB = 0;
    }

    fclose(file);
    return 0;
}

static void perfs_update(perfs_t* perfs)
{
    if (cpu_perf_read(perfs->prevCpuperfs, perfs->cpuAmount) == ERR)
    {
        printf("Failed to read prev CPU perfistics\n");
    }

    nanosleep(SAMPLE_INTERVAL);

    if (cpu_perf_read(perfs->cpuperfs, perfs->cpuAmount) == ERR)
    {
        printf("Failed to read CPU perfistics\n");
    }

    if (mem_perf_read(&perfs->memperfs) == ERR)
    {
        printf("Failed to read memory perfistics\n");
    }
}

static void perf_percentage(clock_t part, clock_t total, uint64_t* whole, uint64_t* thousandths)
{
    if (total == 0)
    {
        *whole = 0;
        *thousandths = 0;
        return;
    }

    uint64_t scaledPart = part * 100000;
    uint64_t percent = scaledPart / total;
    *whole = percent / 1000;
    *thousandths = percent % 1000;
}

static void perfs_print(perfs_t* perfs)
{
    printf("\033[H\033[K\n");

    uint64_t cpuPrefixWidth = 20;
    uint64_t cpuBarWidth = (terminalColumns > cpuPrefixWidth + 1) ? (terminalColumns - cpuPrefixWidth - 1) : PLOT_WIDTH;

    uint64_t memPrefixWidth = 4;
    uint64_t memBarWidth =
        (terminalColumns > memPrefixWidth + 2) ? (terminalColumns - memPrefixWidth - 2) : (PLOT_WIDTH * 2);

    printf("\033[1;33m  CPU Usage:\033[0m\033[K\n");
    for (uint64_t i = 0; i < perfs->cpuAmount; i++)
    {
        clock_t prevTotal = perfs->prevCpuperfs[i].idleClocks + perfs->prevCpuperfs[i].activeClocks +
            perfs->prevCpuperfs[i].interruptClocks;
        clock_t currTotal =
            perfs->cpuperfs[i].idleClocks + perfs->cpuperfs[i].activeClocks + perfs->cpuperfs[i].interruptClocks;

        clock_t totalDelta = currTotal - prevTotal;
        clock_t activeDelta = (perfs->cpuperfs[i].activeClocks - perfs->prevCpuperfs[i].activeClocks) +
            (perfs->cpuperfs[i].interruptClocks - perfs->prevCpuperfs[i].interruptClocks);

        uint64_t whole, thousandths;
        perf_percentage(activeDelta, totalDelta, &whole, &thousandths);

        const char* color;
        if (whole < 30)
        {
            color = "\033[32m";
        }
        else if (whole < 70)
        {
            color = "\033[33m";
        }
        else
        {
            color = "\033[31m";
        }

        printf("  \033[90mCPU%-2llu\033[0m %s%3llu.%03llu%%\033[0m [", i, color, whole, thousandths);

        uint64_t barLength = (whole * cpuBarWidth) / 100;
        for (uint64_t j = 0; j < cpuBarWidth; j++)
        {
            if (j < barLength)
            {
                printf("%s#\033[0m", color);
            }
            else
            {
                printf("\033[90m \033[0m");
            }
        }
        printf("]\033[K\n");
    }

    printf("\033[K\n");

    printf("\033[1;33m  Memory:\033[0m\033[K\n");
    uint64_t usedKiB = perfs->memperfs.totalKiB - perfs->memperfs.freeKiB;
    uint64_t whole, thousandths;
    perf_percentage(usedKiB, perfs->memperfs.totalKiB, &whole, &thousandths);

    const char* color;
    if (whole < 50)
    {
        color = "\033[32m";
    }
    else if (whole < 80)
    {
        color = "\033[33m";
    }
    else
    {
        color = "\033[31m";
    }

    printf("  \033[90mUsed:\033[0m   %s%5llu MiB\033[0m / %5llu MiB  "
           "\033[90m(%s%3llu.%03llu%%\033[0m\033[90m)\033[0m\033[K\n",
        color, usedKiB / 1024, perfs->memperfs.totalKiB / 1024, color, whole, thousandths);
    printf("  \033[90mFree:\033[0m   \033[32m%5llu MiB\033[0m\033[K\n", perfs->memperfs.freeKiB / 1024);

    printf("  [");
    uint64_t barLength = (whole * memBarWidth) / 100;
    for (uint64_t j = 0; j < memBarWidth; j++)
    {
        if (j < barLength)
        {
            printf("%s#\033[0m", color);
        }
        else
        {
            printf("\033[90m \033[0m");
        }
    }
    printf("]\033[K\n");

    printf("\033[K\n");
    fflush(stdout);
}

int main(void)
{
    printf("\033[H\033[J\033[?25l"); // Clear screen and hide cursor

    perfs_t perfs = {0};
    perfs.cpuAmount = cpu_perf_count_cpus();
    if (perfs.cpuAmount == ERR)
    {
        printf("Failed to read CPU perfistics\n");
        return EXIT_FAILURE;
    }
    perfs.prevCpuperfs = calloc(perfs.cpuAmount, sizeof(cpu_perfs_t));
    if (perfs.prevCpuperfs == NULL)
    {
        printf("Failed to allocate memory for previous CPU perfistics\n");
        return EXIT_FAILURE;
    }
    perfs.cpuperfs = calloc(perfs.cpuAmount, sizeof(cpu_perfs_t));
    if (perfs.cpuperfs == NULL)
    {
        printf("Failed to allocate memory for CPU perfistics\n");
        free(perfs.prevCpuperfs);
        return EXIT_FAILURE;
    }
    perfs.memperfs = (mem_perfs_t){0};

    terminalColumns = terminal_columns_get();

    printf("Please wait...\n");
    perfs_update(&perfs);

    while (1)
    {
        perfs_print(&perfs);

        perfs_update(&perfs);
    }

    return 0;
}
