#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <threads.h>
#include <time.h>

typedef struct
{
    uint64_t id;
    clock_t idleClocks;
    clock_t activeClocks;
    clock_t trapClocks;
} cpu_statistics_t;

cpu_statistics_t* cpu_statistics_read(uint64_t* cpuAmount)
{
    FILE* file = fopen("/dev/stat/cpu", "r");
    cpu_statistics_t* stat = NULL;
    (*cpuAmount) = 0;

    char buffer[256];

    fgets(buffer, sizeof(buffer), file); // Discard first line

    while (fgets(buffer, sizeof(buffer), file) != NULL)
    {
        (*cpuAmount)++;
        stat = realloc(stat, sizeof(cpu_statistics_t) * (*cpuAmount));

        cpu_statistics_t* current = &stat[*cpuAmount - 1];
        if (sscanf(buffer, "cpu%d %llu %llu %llu", &current->id, &current->idleClocks, &current->activeClocks,
                &current->trapClocks) == 0)
        {
            break;
        }
    }
    fclose(file);
    return stat;
}

typedef struct
{
    uint64_t total;
    uint64_t free;
    uint64_t reserved;
} mem_statistics_t;

void mem_statistics_read(mem_statistics_t* stats)
{
    FILE* file = fopen("/dev/stat/mem", "r");

    if (fscanf(file, "value kb\ntotal %llu\nfree %llu\nreserved %llu", &stats->total, &stats->free, &stats->reserved) ==
        EOF)
    {
        fprintf(stderr, "failed to read /dev/stat/mem\n");
    }

    fclose(file);
}

int main(void)
{

    uint64_t cpuAmount;
    cpu_statistics_t* before = cpu_statistics_read(&cpuAmount);

    struct timespec timespec = {.tv_nsec = CLOCKS_PER_SEC / 10};
    thrd_sleep(&timespec, NULL);

    cpu_statistics_t* after = cpu_statistics_read(&cpuAmount);

    for (uint64_t i = 0; i < cpuAmount; i++)
    {
        clock_t idleDelta = after[i].idleClocks - before[i].idleClocks;
        clock_t activeDelta = after[i].activeClocks - before[i].activeClocks;
        clock_t trapDelta = after[i].trapClocks - before[i].trapClocks;

        uint64_t totalDelta = activeDelta + trapDelta + idleDelta;
        double percentage = 0;
        if (totalDelta > 0)
        {
            percentage = ((double)(activeDelta + trapDelta) * 100.0) / (double)totalDelta;
        }
        printf("cpu%d %d.%03d%% usage\n", before[i].id, (int)percentage,
            (int)((percentage - (int)percentage) * 1000.0));
    }

    free(before);
    free(after);

    mem_statistics_t stats;
    mem_statistics_read(&stats);

    printf("total memory %d mb\n", stats.total / 1000);
    printf("free memory %d mb\n", stats.free / 1000);
    printf("reserved memory %d mb\n", stats.reserved / 1000);
    printf("used memory %d%%\n", (stats.reserved * 100) / stats.total);

    return 0;
}