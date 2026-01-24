#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fs.h>
#include <sys/proc.h>
#include <threads.h>
#include <time.h>

#define SAMPLE_INTERVAL (CLOCKS_PER_SEC)

static uint64_t terminalColumns;
static uint64_t terminalRows;
static uint64_t processScrollOffset = 0;

static clock_t lastSampleTime = 0;
static uint64_t cpuAmount = 0;

typedef enum
{
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

static uint64_t cpu_perf_read(cpu_perfs_t* cpuPerfs)
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

        if (sscanf(line, "%llu %llu %llu %llu", &cpuPerfs[i].id, &cpuPerfs[i].idleClocks, &cpuPerfs[i].activeClocks,
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
    if (fscanf(file, "total_pages %llu\nfree_pages %llu\nused_pages %llu", &totalPages, &freePages, &usedPages) == 3)
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
    fd_t procDir = open("/proc:directory");
    if (procDir == ERR)
    {
        return NULL;
    }

    proc_perfs_t* procPerfs = NULL;
    *procAmount = 0;
    dirent_t buffer[128];
    while (1)
    {
        size_t readAmount = getdents(procDir, (dirent_t*)buffer, sizeof(buffer));
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
            if (fscanf(perfFile,
                    "user_clocks %llu\nkernel_sched_clocks %llu\nstart_clocks %llu\nuser_pages %lu\nthread_count %llu",
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
    if (pb->cpuPercent > pa->cpuPercent)
        return 1;
    if (pb->cpuPercent < pa->cpuPercent)
        return -1;
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
    clock_t currentTime = clock();
    while (currentTime - lastSampleTime < SAMPLE_INTERVAL)
    {
        clock_t remaining = SAMPLE_INTERVAL - (currentTime - lastSampleTime);
        if (!(poll1(STDIN_FILENO, POLLIN, remaining) & POLLIN))
        {
            break;
        }

        bool keyPressed = true;
        sort_mode_t previousSortMode = currentSortMode;
        uint64_t previousScrollOffset = processScrollOffset;

        char c;
        read(STDIN_FILENO, &c, 1);
        switch (c)
        {
        case 'p':
        case 'P':
            currentSortMode = SORT_PID;
            processScrollOffset = 0;
            break;
        case 'm':
        case 'M':
            currentSortMode = SORT_MEMORY;
            processScrollOffset = 0;
            break;
        case 'c':
        case 'C':
            currentSortMode = SORT_CPU;
            processScrollOffset = 0;
            break;
        case 'j':
        case 'J':
            if (processScrollOffset + 1 < perfs->procAmount)
            {
                processScrollOffset++;
            }
            break;
        case 'k':
        case 'K':
            if (processScrollOffset > 0)
            {
                processScrollOffset--;
            }
            break;
        case 'q':
        case 'Q':
            printf("\033[?25h\033[H\033[J");
            exit(0);
            break;
        default:
            keyPressed = false;
            break;
        }

        if (keyPressed && (previousSortMode != currentSortMode || previousScrollOffset != processScrollOffset))
        {
            sort_processes(perfs);
            break;
        }

        currentTime = clock();
    }

    if (currentTime - lastSampleTime < SAMPLE_INTERVAL)
    {
        return;
    }

    if (perfs->cpuPerfs != NULL)
    {
        memcpy(perfs->prevCpuPerfs, perfs->cpuPerfs, sizeof(cpu_perfs_t) * cpuAmount);
    }
    free(perfs->prevProcPerfs);
    perfs->prevProcPerfs = perfs->procPerfs;
    perfs->prevProcAmount = perfs->procAmount;

    if (cpu_perf_read(perfs->cpuPerfs) == ERR)
    {
        printf("Failed to read CPU performance data\n");
        abort();
    }

    if (mem_perf_read(&perfs->memPerfs) == ERR)
    {
        printf("Failed to read memory performance data\n");
        abort();
    }

    perfs->procPerfs = proc_perfs_read(&perfs->procAmount);
    if (perfs->procPerfs == NULL)
    {
        printf("Failed to read process performance data\n");
        abort();
    }

    calculate_cpu_percentages(perfs);
    sort_processes(perfs);

    lastSampleTime = currentTime;
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
    printf("\033[H\n");

    uint64_t cpuPrefixWidth = 20;
    uint64_t singleColumnWidth = (terminalColumns + 1) / 2;
    uint64_t cpuBarWidth = singleColumnWidth - cpuPrefixWidth;

    uint64_t memPrefixWidth = 4;
    uint64_t memBarWidth = terminalColumns - memPrefixWidth - 2;

    printf("\033[1;33m  CPU Usage:\033[0m\033[K\n");

    uint64_t cpusPerColumn = (cpuAmount + 1) / 2;
    for (uint64_t row = 0; row < cpusPerColumn; row++)
    {
        uint64_t i = row;
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
        printf("]");

        uint64_t rightIdx = row + cpusPerColumn;
        if (rightIdx < cpuAmount)
        {
            prevTotal = perfs->prevCpuPerfs[rightIdx].idleClocks + perfs->prevCpuPerfs[rightIdx].activeClocks +
                perfs->prevCpuPerfs[rightIdx].interruptClocks;
            currTotal = perfs->cpuPerfs[rightIdx].idleClocks + perfs->cpuPerfs[rightIdx].activeClocks +
                perfs->cpuPerfs[rightIdx].interruptClocks;

            totalDelta = currTotal - prevTotal;
            activeDelta = (perfs->cpuPerfs[rightIdx].activeClocks - perfs->prevCpuPerfs[rightIdx].activeClocks) +
                (perfs->cpuPerfs[rightIdx].interruptClocks - perfs->prevCpuPerfs[rightIdx].interruptClocks);

            perf_percentage(activeDelta, totalDelta, &whole, &thousandths);

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

            printf("  \033[90mCPU%-2llu\033[0m %s%3llu.%03llu%%\033[0m [", rightIdx, color, whole, thousandths);

            barLength = (whole * cpuBarWidth) / 100;
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
            printf("]");
        }

        printf("\033[K\n");
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

    printf("  Processes:\033[0m%s  \033[90m(p=PID, m=Mem, c=CPU, j/k=scroll)\033[0m\033[K\n", sortIndicator);
    printf("  \033[90mPID         CPU%%     KiB    Threads  Command\033[0m\033[K\n");
    printf("  \033[90m");
    for (uint64_t i = 0; i < terminalColumns - 4; i++)
    {
        printf("-");
    }
    printf("\n\033[0m");

    uint64_t headerLines = 4 + cpusPerColumn + 7;
    uint64_t availableLines = (terminalRows - 1 > headerLines + 1) ? (terminalRows - 1 - headerLines - 1) : 10;

    if (processScrollOffset > perfs->procAmount)
    {
        processScrollOffset = perfs->procAmount;
    }
    if (perfs->procAmount > availableLines && processScrollOffset > perfs->procAmount - availableLines)
    {
        processScrollOffset = perfs->procAmount - availableLines;
    }

    uint64_t displayCount = availableLines;
    if (processScrollOffset + displayCount > perfs->procAmount)
    {
        displayCount = perfs->procAmount - processScrollOffset;
    }

    for (uint64_t i = 0; i < displayCount; i++)
    {
        uint64_t procIdx = processScrollOffset + i;
        const char* cpuColor;
        if (perfs->procPerfs[procIdx].cpuPercent < 10.0)
        {
            cpuColor = "\033[32m";
        }
        else if (perfs->procPerfs[procIdx].cpuPercent < 50.0)
        {
            cpuColor = "\033[33m";
        }
        else
        {
            cpuColor = "\033[31m";
        }

        const char* memColor;
        if (perfs->procPerfs[procIdx].userKiB < 1024 * 50)
        {
            memColor = "\033[32m";
        }
        else if (perfs->procPerfs[procIdx].userKiB < 1024 * 200)
        {
            memColor = "\033[33m";
        }
        else
        {
            memColor = "\033[31m";
        }

        uint64_t cpuWhole = (uint64_t)perfs->procPerfs[procIdx].cpuPercent;
        uint64_t cpuThousandths = (uint64_t)((perfs->procPerfs[procIdx].cpuPercent - cpuWhole) * 1000);

        char displayCmdline[terminalColumns - 40 + 1];
        if (strlen(perfs->procPerfs[procIdx].cmdline) > terminalColumns - 40)
        {
            strncpy(displayCmdline, perfs->procPerfs[procIdx].cmdline, terminalColumns - 43);
            displayCmdline[terminalColumns - 43] = '\0';
            strcat(displayCmdline, "...");
        }
        else
        {
            strcpy(displayCmdline, perfs->procPerfs[procIdx].cmdline);
        }

        printf("  \033[90m%-8d\033[0m %s%4llu.%03llu%%\033[0m %s%7llu\033[0m  %7llu  %s\033[K\n",
            perfs->procPerfs[procIdx].pid, cpuColor, cpuWhole, cpuThousandths, memColor,
            perfs->procPerfs[procIdx].userKiB, perfs->procPerfs[procIdx].threadCount, displayCmdline);
    }

    for (uint64_t i = displayCount; i < availableLines; i++)
    {
        printf("\033[K\n");
    }

    fflush(stdout);
}

int main(void)
{
    cpuAmount = cpu_perf_count_cpus();
    if (cpuAmount == ERR)
    {
        printf("Failed to read CPU amount\n");
        abort();
    }

    perfs_t perfs = {0};
    perfs.prevCpuPerfs = calloc(cpuAmount, sizeof(cpu_perfs_t));
    if (perfs.prevCpuPerfs == NULL)
    {
        printf("Failed to allocate memory for previous CPU performance data\n");
        return EXIT_FAILURE;
    }
    perfs.cpuPerfs = calloc(cpuAmount, sizeof(cpu_perfs_t));
    if (perfs.cpuPerfs == NULL)
    {
        printf("Failed to allocate memory for CPU performance data\n");
        free(perfs.prevCpuPerfs);
        return EXIT_FAILURE;
    }
    perfs.prevProcPerfs = NULL;
    perfs.procPerfs = NULL;
    perfs.memPerfs = (mem_perfs_t){0};

    terminal_size_get();

    bool pleaseWaitShown = true;
    const char* waitMessage = "[Please Wait]";
    uint64_t waitMessageLength = strlen(waitMessage);
    printf("\033[H\033[J\033[?25l\033[%lluC%s\n", (terminalColumns - waitMessageLength) / 2, waitMessage);

    while (1)
    {
        perfs_print(&perfs);
        perfs_update(&perfs);
        if (pleaseWaitShown)
        {
            printf("\033[s\033[H\033[K\033[u");
            pleaseWaitShown = false;
        }
    }

    printf("\033[?25h");
    return 0;
}
