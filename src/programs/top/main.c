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
} cpu_metrics_t;

cpu_metrics_t* cpu_metrics_read(uint64_t* cpuAmount)
{
    FILE* file = fopen("sys:/metrics/cpu", "r");
    cpu_metrics_t* metrics = NULL;
    (*cpuAmount) = 0;

    char buffer[256];

    fgets(buffer, sizeof(buffer), file); // Discard first line

    while (fgets(buffer, sizeof(buffer), file) != NULL)
    {
        (*cpuAmount)++;
        metrics = realloc(metrics, sizeof(cpu_metrics_t) * (*cpuAmount));

        cpu_metrics_t* current = &metrics[*cpuAmount - 1];
        if (sscanf(buffer, "cpu%d %llu %llu %llu", &current->id, &current->idleClocks, &current->activeClocks,
                &current->trapClocks) == 0)
        {
            break;
        }
    }
    fclose(file);
    return metrics;
}

int main(void)
{
    uint64_t cpuAmount;
    cpu_metrics_t* before = cpu_metrics_read(&cpuAmount);

    struct timespec timespec = {.tv_sec = 1};
    thrd_sleep(&timespec, NULL);

    cpu_metrics_t* after = cpu_metrics_read(&cpuAmount);

    for (uint64_t i = 0; i < cpuAmount; i++)
    {
        clock_t idleDelta = after[i].idleClocks - before[i].idleClocks;
        clock_t activeDelta = after[i].activeClocks - before[i].activeClocks;
        clock_t trapDelta = after[i].trapClocks - before[i].trapClocks;
        
        uint64_t totalDelta = activeDelta + trapDelta + idleDelta;
        uint64_t percentage = ((activeDelta + trapDelta) * 100) / totalDelta;
        printf("cpu%d %d.%02d%% usage\n", before[i].id, percentage / 100, percentage % 100);
    }

    return 0;
}