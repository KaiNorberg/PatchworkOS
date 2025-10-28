#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <threads.h>
#include <time.h>
#include <sys/proc.h>

#define SAMPLE_INTERVAL (CLOCKS_PER_SEC)

#define PLOT_WIDTH 80
#define PLOT_HEIGHT 10

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
    uint8_t totalCpuHistory[PLOT_WIDTH];
    uint8_t memHistory[PLOT_WIDTH];
    uint8_t historyIndex;
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

        if (sscanf(line, "cpu%d %llu %llu %llu",
               &cpuStats[i].id,
               &cpuStats[i].idleClocks,
               &cpuStats[i].activeClocks,
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

    if (fscanf(file, "value kib\ntotal %llu\nfree %llu\nreserved %llu",
                &memStats->totalKiB, &memStats->freeKiB, &memStats->reservedKiB) != 3)
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

static void stats_big_plot_print(const char* title, uint8_t* history, uint8_t historyIndex, uint64_t maxValue)
{
    printf("%s\n", title);
    for (int32_t row = PLOT_HEIGHT - 1; row >= 0; row--)
    {
        for (uint64_t col = 0; col < PLOT_WIDTH; col++)
        {
            uint64_t index = (historyIndex + col + 1) % PLOT_WIDTH;
            uint8_t value = history[index];
            uint8_t threshold = (uint8_t)(((row + 1) * maxValue) / PLOT_HEIGHT);
            if (value >= threshold)
            {
                printf("@");
            }
            else
            {
                printf(" ");
            }
        }
        printf("\n");
    }
    printf("\n");
}

static void stats_print(stats_t* stats)
{
    printf("\033[H");

    clock_t totalIdle = 0;
    clock_t totalActive = 0;
    clock_t totalInterrupt = 0;
    for (uint64_t i = 0; i < stats->cpuAmount; i++)
    {
        clock_t idleDelta = stats->cpuStats[i].idleClocks - stats->prevCpuStats[i].idleClocks;
        clock_t activeDelta = stats->cpuStats[i].activeClocks - stats->prevCpuStats[i].activeClocks;
        clock_t interruptDelta = stats->cpuStats[i].interruptClocks - stats->prevCpuStats[i].interruptClocks;

        totalIdle += idleDelta;
        totalActive += activeDelta;
        totalInterrupt += interruptDelta;
    }

    clock_t totalDelta = totalIdle + totalActive + totalInterrupt;
    uint64_t idleWhole, idleThousandths;
    uint64_t activeWhole, activeThousandths;
    uint64_t interruptWhole, interruptThousandths;
    stat_percentage(totalIdle, totalDelta, &idleWhole, &idleThousandths);
    stat_percentage(totalActive, totalDelta, &activeWhole, &activeThousandths);
    stat_percentage(totalInterrupt, totalDelta, &interruptWhole, &interruptThousandths);

    //uint8_t cpuUsage = (uint8_t)((totalActive * 100) / totalDelta);
    //stats->totalCpuHistory[stats->historyIndex] = cpuUsage;

    //stats_big_plot_print("CPU Usage (%)", stats->totalCpuHistory, stats->historyIndex, 100);
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
    stats.historyIndex = 0;

    while (1)
    {
        stats_print(&stats);

        stats_update(&stats);
    }

    return 0;
}
