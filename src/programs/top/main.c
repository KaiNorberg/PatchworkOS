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
} cpu_stats_t;

typedef struct
{
    uint64_t totalKiB;
    uint64_t freeKiB;
    uint64_t reservedKiB;
} mem_stats_t;

typedef struct
{
    uint64_t cpuAmount;
    cpu_stats_t* prevCpuStats;
    cpu_stats_t* cpuStats;
    mem_stats_t memStats;
} stats_t;

static uint64_t cpu_stat_count_cpus(void)
{
    FILE* file = fopen("/dev/stat/cpu", "r");
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

static uint64_t cpu_stat_read(cpu_stats_t* cpuStats, uint64_t cpuAmount)
{
    FILE* file = fopen("/dev/stat/cpu", "r");
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

        if (sscanf(line, "cpu%d %llu %llu %llu", &cpuStats[i].id, &cpuStats[i].idleClocks, &cpuStats[i].activeClocks,
                &cpuStats[i].interruptClocks) != 4)
        {
            cpuStats[i].id = 0;
            cpuStats[i].idleClocks = 0;
            cpuStats[i].activeClocks = 0;
            cpuStats[i].interruptClocks = 0;
        }
    }

    fclose(file);
    return 0;
}

static uint64_t mem_stat_read(mem_stats_t* memStats)
{
    FILE* file = fopen("/dev/stat/mem", "r");
    if (file == NULL)
    {
        return ERR;
    }

    if (fscanf(file, "value kib\ntotal %llu\nfree %llu\nreserved %llu", &memStats->totalKiB, &memStats->freeKiB,
            &memStats->reservedKiB) != 3)
    {
        memStats->totalKiB = 0;
        memStats->freeKiB = 0;
        memStats->reservedKiB = 0;
    }

    fclose(file);
    return 0;
}

static void stats_update(stats_t* stats)
{
    if (cpu_stat_read(stats->prevCpuStats, stats->cpuAmount) == ERR)
    {
        printf("Failed to read prev CPU statistics\n");
    }

    nanosleep(SAMPLE_INTERVAL);

    if (cpu_stat_read(stats->cpuStats, stats->cpuAmount) == ERR)
    {
        printf("Failed to read CPU statistics\n");
    }

    if (mem_stat_read(&stats->memStats) == ERR)
    {
        printf("Failed to read memory statistics\n");
    }
}

static void stat_percentage(clock_t part, clock_t total, uint64_t* whole, uint64_t* thousandths)
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

static void stats_print(stats_t* stats)
{
    printf("\033[H\033[K\n");

    uint64_t cpuPrefixWidth = 20;
    uint64_t cpuBarWidth = (terminalColumns > cpuPrefixWidth + 1) ?
                           (terminalColumns - cpuPrefixWidth - 1) : PLOT_WIDTH;

    uint64_t memPrefixWidth = 4;
    uint64_t memBarWidth = (terminalColumns > memPrefixWidth + 2) ?
                           (terminalColumns - memPrefixWidth - 2) : (PLOT_WIDTH * 2);

    printf("\033[1;33m  CPU Usage:\033[0m\033[K\n");
    for (uint64_t i = 0; i < stats->cpuAmount; i++)
    {
        clock_t prevTotal = stats->prevCpuStats[i].idleClocks + stats->prevCpuStats[i].activeClocks +
            stats->prevCpuStats[i].interruptClocks;
        clock_t currTotal =
            stats->cpuStats[i].idleClocks + stats->cpuStats[i].activeClocks + stats->cpuStats[i].interruptClocks;

        clock_t totalDelta = currTotal - prevTotal;
        clock_t activeDelta = (stats->cpuStats[i].activeClocks - stats->prevCpuStats[i].activeClocks) +
            (stats->cpuStats[i].interruptClocks - stats->prevCpuStats[i].interruptClocks);

        uint64_t whole, thousandths;
        stat_percentage(activeDelta, totalDelta, &whole, &thousandths);

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
    uint64_t usedKiB = stats->memStats.totalKiB - stats->memStats.freeKiB;
    uint64_t whole, thousandths;
    stat_percentage(usedKiB, stats->memStats.totalKiB, &whole, &thousandths);

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
        color, usedKiB / 1024, stats->memStats.totalKiB / 1024, color, whole, thousandths);
    printf("  \033[90mFree:\033[0m   \033[32m%5llu MiB\033[0m\033[K\n", stats->memStats.freeKiB / 1024);

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
    printf("\033[H\033[J");

    stats_t stats = {0};
    stats.cpuAmount = cpu_stat_count_cpus();
    if (stats.cpuAmount == ERR)
    {
        printf("Failed to read CPU statistics\n");
        return EXIT_FAILURE;
    }
    stats.prevCpuStats = calloc(stats.cpuAmount, sizeof(cpu_stats_t));
    if (stats.prevCpuStats == NULL)
    {
        printf("Failed to allocate memory for previous CPU statistics\n");
        return EXIT_FAILURE;
    }
    stats.cpuStats = calloc(stats.cpuAmount, sizeof(cpu_stats_t));
    if (stats.cpuStats == NULL)
    {
        printf("Failed to allocate memory for CPU statistics\n");
        free(stats.prevCpuStats);
        return EXIT_FAILURE;
    }
    stats.memStats = (mem_stats_t){0};

    terminalColumns = terminal_columns_get();

    printf("Please wait...\n");
    stats_update(&stats);

    while (1)
    {
        stats_print(&stats);

        stats_update(&stats);
    }

    return 0;
}
