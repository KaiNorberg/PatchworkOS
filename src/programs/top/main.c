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
static uint64_t terminalRows;

typedef enum {
    SORT_PID,
    SORT_MEMORY,
    SORT_CPU
} sort_mode_t;

static sort_mode_t currentSortMode = SORT_CPU;

static void terminal_size_get(void)
{
    uint32_t terminalWidth = 80;
    uint32_t terminalHeight = 24;

    printf("\033[s\033[999;999H\033[6n");
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

    int rows, cols;
    sscanf(buffer, "\033[%d;%dR", &rows, &cols);

    if (cols != 0)
    {
        terminalWidth = (uint32_t)cols;
    }
    if (rows != 0)
    {
        terminalHeight = (uint32_t)rows;
    }

    printf("\033[H\033[u");
    fflush(stdout);

    printf("Detected terminal size: %ux%u\n", terminalWidth, terminalHeight);

    terminalColumns = terminalWidth;
    terminalRows = terminalHeight;
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
    uint64_t usedKiB;
} mem_perfs_t;

typedef struct
{
    pid_t pid;
    clock_t userClocks;
    clock_t kernelClocks;
    clock_t startClocks;
    uint64_t userKiB;
    uint64_t threadCount;
    double cpuPercent;
    char cmdline[256];
} proc_perfs_t;

typedef struct
{
    uint64_t cpuAmount;
    cpu_perfs_t* prevCpuPerfs;
    cpu_perfs_t* cpuPerfs;
    uint64_t prevProcAmount;
    proc_perfs_t* prevProcPerfs;
    uint64_t procAmount;
    proc_perfs_t* procPerfs;
    mem_perfs_t memPerfs;
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
    return cpuCount - 1;
}

static uint64_t cpu_perf_read(cpu_perfs_t* cpuPerfs, uint64_t cpuAmount)
{
    FILE* file = fopen("/dev/perf/cpu", "r");
    if (file == NULL)
    {
        return ERR;
    }

    char line[1024];
    fgets(line, sizeof(line), file);

    for (uint64_t i = 0; i < cpuAmount; i++)
    {
        if (fgets(line, sizeof(line), file) == NULL)
        {
            break;
        }

        if (sscanf(line, "%lu %lu %lu %lu", &cpuPerfs[i].id, &cpuPerfs[i].idleClocks, &cpuPerfs[i].activeClocks,
                &cpuPerfs[i].interruptClocks) != 4)
        {
            cpuPerfs[i].id = 0;
            cpuPerfs[i].idleClocks = 0;
            cpuPerfs[i].activeClocks = 0;
            cpuPerfs[i].interruptClocks = 0;
        }
    }

    fclose(file);
    return 0;
}

static uint64_t mem_perf_read(mem_perfs_t* memPerfs)
{
    FILE* file = fopen("/dev/perf/mem", "r");
    if (file == NULL)
    {
        return ERR;
    }

    uint64_t totalPages = 0;
    uint64_t freePages = 0;
    uint64_t usedPages = 0;
    if (fscanf(file, "total_pages %lu\nfree_pages %lu\nused_pages %lu", &totalPages, &freePages, &usedPages) == 3)
    {
        memPerfs->totalKiB = totalPages * (PAGE_SIZE / 1024);
        memPerfs->freeKiB = freePages * (PAGE_SIZE / 1024);
        memPerfs->usedKiB = usedPages * (PAGE_SIZE / 1024);
    }
    else
    {
        memPerfs->totalKiB = 0;
        memPerfs->freeKiB = 0;
        memPerfs->usedKiB = 0;
    }

    fclose(file);
    return 0;
}

static proc_perfs_t* proc_perfs_read(uint64_t* procAmount)
{
    fd_t procDir = open("/proc:dir");
    if (procDir == ERR)
    {
        return NULL;
    }

    proc_perfs_t* procPerfs = NULL;
    *procAmount = 0;
    dirent_t buffer[128];
    while (1)
    {
        uint64_t readAmount = getdents(procDir, (dirent_t*)buffer, sizeof(buffer));
        if (readAmount == ERR)
        {
            free(procPerfs);
            close(procDir);
            return NULL;
        }
        if (readAmount == 0)
        {
            break;
        }

        for (uint64_t i = 0; i < readAmount / sizeof(dirent_t); i++)
        {
            if (buffer[i].path[0] == '.' || strcmp(buffer[i].path, "self") == 0)
            {
                continue;
            }

            pid_t pid = (pid_t)atoi(buffer[i].path);
            if (pid == 0)
            {
                continue;
            }

            char path[MAX_PATH];
            snprintf(path, sizeof(path), "/proc/%d/perf", pid);
            FILE* perfFile = fopen(path, "r");
            if (perfFile == NULL)
            {
                continue;
            }

            uint64_t userPages;
            proc_perfs_t procPerf;
            if (fscanf(perfFile, "user_clocks %lu\nkernel_clocks %lu\nstart_clocks %lu\nuser_pages %lu\nthread_count %lu",
                    &procPerf.userClocks, &procPerf.kernelClocks, &procPerf.startClocks, &userPages,
                    &procPerf.threadCount) != 5)
            {
                fclose(perfFile);
                continue;
            }
            procPerf.pid = pid;
            procPerf.userKiB = userPages * (PAGE_SIZE / 1024);
            procPerf.cpuPercent = 0.0;
            fclose(perfFile);

            snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
            FILE* cmdlineFile = fopen(path, "r");
            if (cmdlineFile == NULL)
            {
                free(procPerfs);
                close(procDir);
                return NULL;
            }

            char c;
            uint64_t index = 0;
            while (fread(&c, 1, 1, cmdlineFile) == 1 && index < sizeof(procPerf.cmdline) - 1)
            {
                if (c == '\0')
                {
                    procPerf.cmdline[index] = ' ';
                }
                else
                {
                    procPerf.cmdline[index] = c;
                }
                index++;
            }
            procPerf.cmdline[index] = '\0';
            fclose(cmdlineFile);

            procPerfs = realloc(procPerfs, sizeof(proc_perfs_t) * (*procAmount + 1));
            if (procPerfs == NULL)
            {
                close(procDir);
                return NULL;
            }
            procPerfs[(*procAmount)++] = procPerf;
        }
    }

    close(procDir);
    return procPerfs;
}

static void calculate_cpu_percentages(perfs_t* perfs)
{
    for (uint64_t i = 0; i < perfs->procAmount; i++)
    {
        perfs->procPerfs[i].cpuPercent = 0.0;

        for (uint64_t j = 0; j < perfs->prevProcAmount; j++)
        {
            if (perfs->procPerfs[i].pid == perfs->prevProcPerfs[j].pid)
            {
                clock_t userDelta = perfs->procPerfs[i].userClocks - perfs->prevProcPerfs[j].userClocks;
                clock_t kernelDelta = perfs->procPerfs[i].kernelClocks - perfs->prevProcPerfs[j].kernelClocks;
                clock_t totalDelta = userDelta + kernelDelta;

                perfs->procPerfs[i].cpuPercent = (totalDelta * 100.0) / SAMPLE_INTERVAL;
                break;
            }
        }
    }
}

static int compare_by_pid(const void* a, const void* b)
{
    const proc_perfs_t* pa = (const proc_perfs_t*)a;
    const proc_perfs_t* pb = (const proc_perfs_t*)b;
    return (pa->pid > pb->pid) - (pa->pid < pb->pid);
}

static int compare_by_memory(const void* a, const void* b)
{
    const proc_perfs_t* pa = (const proc_perfs_t*)a;
    const proc_perfs_t* pb = (const proc_perfs_t*)b;
    return (pb->userKiB > pa->userKiB) - (pb->userKiB < pa->userKiB);
}

static int compare_by_cpu(const void* a, const void* b)
{
    const proc_perfs_t* pa = (const proc_perfs_t*)a;
    const proc_perfs_t* pb = (const proc_perfs_t*)b;
    if (pb->cpuPercent > pa->cpuPercent) return 1;
    if (pb->cpuPercent < pa->cpuPercent) return -1;
    return 0;
}

static void sort_processes(perfs_t* perfs)
{
    switch (currentSortMode)
    {
        case SORT_PID:
            qsort(perfs->procPerfs, perfs->procAmount, sizeof(proc_perfs_t), compare_by_pid);
            break;
        case SORT_MEMORY:
            qsort(perfs->procPerfs, perfs->procAmount, sizeof(proc_perfs_t), compare_by_memory);
            break;
        case SORT_CPU:
            qsort(perfs->procPerfs, perfs->procAmount, sizeof(proc_perfs_t), compare_by_cpu);
            break;
    }
}

static void perfs_update(perfs_t* perfs)
{
    if (cpu_perf_read(perfs->prevCpuPerfs, perfs->cpuAmount) == ERR)
    {
        printf("Failed to read prev CPU performance data\n");
        abort();
    }

    free(perfs->prevProcPerfs);
    perfs->prevProcPerfs = proc_perfs_read(&perfs->prevProcAmount);
    if (perfs->prevProcPerfs == NULL)
    {
        printf("Failed to read prev process performance data\n");
        abort();
    }

    if (poll1(STDIN_FILENO, POLLIN, SAMPLE_INTERVAL) > 0)
    {
        char c;
        read(STDIN_FILENO, &c, 1);
        switch (c)
        {
            case 'p':
            case 'P':
                currentSortMode = SORT_PID;
                break;
            case 'm':
            case 'M':
                currentSortMode = SORT_MEMORY;
                break;
            case 'c':
            case 'C':
                currentSortMode = SORT_CPU;
                break;
            case 'q':
            case 'Q':
                printf("\033[?25h\033[H\033[J");
                exit(0);
                break;
        }
    }

    if (cpu_perf_read(perfs->cpuPerfs, perfs->cpuAmount) == ERR)
    {
        printf("Failed to read CPU performance data\n");
        abort();
    }

    if (mem_perf_read(&perfs->memPerfs) == ERR)
    {
        printf("Failed to read memory performance data\n");
        abort();
    }

    free(perfs->procPerfs);
    perfs->procPerfs = proc_perfs_read(&perfs->procAmount);
    if (perfs->procPerfs == NULL)
    {
        printf("Failed to read process performance data\n");
        abort();
    }

    calculate_cpu_percentages(perfs);
    sort_processes(perfs);
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
    printf("\033[H");

    uint64_t cpuPrefixWidth = 20;
    uint64_t cpuBarWidth = (terminalColumns > cpuPrefixWidth + 1) ? (terminalColumns - cpuPrefixWidth - 1) : PLOT_WIDTH;

    uint64_t memPrefixWidth = 4;
    uint64_t memBarWidth =
        (terminalColumns > memPrefixWidth + 2) ? (terminalColumns - memPrefixWidth - 2) : (PLOT_WIDTH * 2);

    printf("\033[1;33m  CPU Usage:\033[0m\033[K\n");
    for (uint64_t i = 0; i < perfs->cpuAmount; i++)
    {
        clock_t prevTotal = perfs->prevCpuPerfs[i].idleClocks + perfs->prevCpuPerfs[i].activeClocks +
            perfs->prevCpuPerfs[i].interruptClocks;
        clock_t currTotal =
            perfs->cpuPerfs[i].idleClocks + perfs->cpuPerfs[i].activeClocks + perfs->cpuPerfs[i].interruptClocks;

        clock_t totalDelta = currTotal - prevTotal;
        clock_t activeDelta = (perfs->cpuPerfs[i].activeClocks - perfs->prevCpuPerfs[i].activeClocks) +
            (perfs->cpuPerfs[i].interruptClocks - perfs->prevCpuPerfs[i].interruptClocks);

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
    uint64_t usedKiB = perfs->memPerfs.totalKiB - perfs->memPerfs.freeKiB;
    uint64_t whole, thousandths;
    perf_percentage(usedKiB, perfs->memPerfs.totalKiB, &whole, &thousandths);

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
        color, usedKiB / 1024, perfs->memPerfs.totalKiB / 1024, color, whole, thousandths);
    printf("  \033[90mFree:\033[0m   \033[32m%5llu MiB\033[0m\033[K\n", perfs->memPerfs.freeKiB / 1024);

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

    const char* sortIndicator = "";
    switch (currentSortMode)
    {
        case SORT_PID:
            sortIndicator = " \033[36m[PID]\033[0m";
            break;
        case SORT_MEMORY:
            sortIndicator = " \033[36m[MEM]\033[0m";
            break;
        case SORT_CPU:
            sortIndicator = " \033[36m[CPU]\033[0m";
            break;
    }

    printf("\033[33m  Processes:\033[0m%s  \033[90m(sort keybindings: p=PID, m=Memory, c=CPU)\033[0m\033[K\n", sortIndicator);
    printf("  \033[90m%-8s %8s %8s %8s\033[0m\033[K\n", "PID", "CPU", "MEM(KiB)", "THREADS");
    printf("  \033[90m");
    for (uint64_t i = 0; i < (terminalColumns > 38 ? 38 : terminalColumns - 2); i++)
    {
        printf("-");
    }
    printf("\033[0m\033[K\n");

    uint64_t headerLines = 4 + perfs->cpuAmount + 7;
    uint64_t availableLines = (terminalRows > headerLines + 1) ? (terminalRows - headerLines - 1) : 10;
    uint64_t displayCount = (perfs->procAmount < availableLines) ? perfs->procAmount : availableLines;

    for (uint64_t i = 0; i < displayCount; i++)
    {
        const char* cpuColor;
        if (perfs->procPerfs[i].cpuPercent < 10.0)
        {
            cpuColor = "\033[32m";
        }
        else if (perfs->procPerfs[i].cpuPercent < 50.0)
        {
            cpuColor = "\033[33m";
        }
        else
        {
            cpuColor = "\033[31m";
        }

        const char* memColor;
        if (perfs->procPerfs[i].userKiB < 1024 * 50)
        {
            memColor = "\033[32m";
        }
        else if (perfs->procPerfs[i].userKiB < 1024 * 200)
        {
            memColor = "\033[33m";
        }
        else
        {
            memColor = "\033[31m";
        }

        uint64_t whole = (uint64_t)perfs->procPerfs[i].cpuPercent;
        uint64_t thousandths = (uint64_t)((perfs->procPerfs[i].cpuPercent - whole) * 1000);

        char displayCmdline[terminalColumns - 40 + 1];
        if (strlen(perfs->procPerfs[i].cmdline) > terminalColumns - 40)
        {
            strncpy(displayCmdline, perfs->procPerfs[i].cmdline, terminalColumns - 43);
            displayCmdline[terminalColumns - 43] = '\0';
            strcat(displayCmdline, "...");
        }
        else
        {
            strcpy(displayCmdline, perfs->procPerfs[i].cmdline);
        }

        printf("  \033[90m%-8d\033[0m %s%3llu.%03llu%%\033[0m %s%7llu\033[0m  %7llu  %s\033[K\n",
            perfs->procPerfs[i].pid,
            cpuColor, whole, thousandths,
            memColor, perfs->procPerfs[i].userKiB,
            perfs->procPerfs[i].threadCount,
            displayCmdline);
    }

    for (uint64_t i = displayCount; i < availableLines; i++)
    {
        printf("\033[K\n");
    }

    fflush(stdout);
}

int main(void)
{
    perfs_t perfs = {0};
    perfs.cpuAmount = cpu_perf_count_cpus();
    if (perfs.cpuAmount == ERR)
    {
        printf("Failed to read CPU performance data\n");
        return EXIT_FAILURE;
    }
    perfs.prevCpuPerfs = calloc(perfs.cpuAmount, sizeof(cpu_perfs_t));
    if (perfs.prevCpuPerfs == NULL)
    {
        printf("Failed to allocate memory for previous CPU performance data\n");
        return EXIT_FAILURE;
    }
    perfs.cpuPerfs = calloc(perfs.cpuAmount, sizeof(cpu_perfs_t));
    if (perfs.cpuPerfs == NULL)
    {
        printf("Failed to allocate memory for CPU performance data\n");
        free(perfs.prevCpuPerfs);
        return EXIT_FAILURE;
    }
    perfs.prevProcPerfs = NULL;
    perfs.procPerfs = NULL;
    perfs.memPerfs = (mem_perfs_t){0};

    printf("Please wait...\n");
    terminal_size_get();
    perfs_update(&perfs);

    printf("\033[H\033[J\033[?25l");

    while (1)
    {
        perfs_print(&perfs);
        perfs_update(&perfs);
    }

    printf("\033[?25h");
    return 0;
}
