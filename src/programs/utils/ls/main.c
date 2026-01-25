#define __STDC_WANT_LIB_EXT1__ 1
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fs.h>

static bool showAll = false;
static bool showFlags = false;

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

static int dirent_cmp(const void* a, const void* b)
{
    const dirent_t* da = (const dirent_t*)a;
    const dirent_t* db = (const dirent_t*)b;
    return strcmp(da->path, db->path);
}

static uint64_t print_dir(const char* path)
{
    fd_t fd = open(path);
    if (fd == _FAIL)
    {
        fprintf(stderr, "ls: can't open directory %s (%s)\n", path, strerror(errno));
        return _FAIL;
    }

    uint64_t capacity = 16;
    uint64_t count = 0;
    dirent_t* entries = malloc(sizeof(dirent_t) * capacity);
    if (entries == NULL)
    {
        close(fd);
        fprintf(stderr, "ls: memory allocation failed\n");
        return _FAIL;
    }

    while (true)
    {
        if (count == capacity)
        {
            capacity *= 2;
            dirent_t* newEntries = realloc(entries, sizeof(dirent_t) * capacity);
            if (newEntries == NULL)
            {
                free(entries);
                close(fd);
                fprintf(stderr, "ls: memory allocation failed\n");
                return _FAIL;
            }
            entries = newEntries;
        }

        uint64_t bytesRead = getdents(fd, &entries[count], sizeof(dirent_t) * (capacity - count));
        if (bytesRead == _FAIL)
        {
            free(entries);
            close(fd);
            fprintf(stderr, "ls: can't read directory %s (%s)\n", path, strerror(errno));
            return _FAIL;
        }
        if (bytesRead == 0)
        {
            break;
        }

        count += bytesRead / sizeof(dirent_t);
    }
    close(fd);

    if (count == 0)
    {
        free(entries);
        return 0;
    }

    if (!showAll)
    {
        uint64_t newCount = 0;
        for (uint64_t i = 0; i < count; i++)
        {
            if (entries[i].path[0] != '.' && strstr(entries[i].path, "/.") == NULL)
            {
                if (i != newCount)
                {
                    entries[newCount] = entries[i];
                }
                newCount++;
            }
        }
        count = newCount;
        if (count == 0)
        {
            free(entries);
            return 0;
        }
    }

    qsort(entries, count, sizeof(dirent_t), dirent_cmp);

    uint64_t maxLen = 0;
    for (uint64_t i = 0; i < count; i++)
    {
        uint64_t len = strlen(entries[i].path);
        if (entries[i].type == VDIR || entries[i].type == VSYMLINK)
        {
            len++;
        }
        if (showFlags)
        {
            len += strlen(entries[i].mode);
        }
        if (len > maxLen)
        {
            maxLen = len;
        }
    }

    uint32_t termWidth = terminal_columns_get();
    uint32_t colWidth = maxLen + 2;
    if (colWidth > termWidth)
    {
        colWidth = termWidth;
    }

    uint32_t numCols = termWidth / colWidth;
    if (numCols == 0)
    {
        numCols = 1;
    }

    uint32_t numRows = (count + numCols - 1) / numCols;

    for (uint32_t r = 0; r < numRows; r++)
    {
        for (uint32_t c = 0; c < numCols; c++)
        {
            uint32_t index = c * numRows + r;
            if (index >= count)
            {
                continue;
            }

            const dirent_t* ent = &entries[index];
            const char* name = ent->path;
            int len = strlen(name);
            const char* modifier = (ent->flags & DIRENT_MOUNTED) ? "\033[4m" : "";

            if (ent->type == VDIR)
            {
                printf("%s\033[34m%s%s\033[0m/", modifier, name, showFlags ? ent->mode : "");
                len++;
            }
            else if (ent->type == VSYMLINK)
            {
                printf("%s\033[36m%s%s\033[0m@", modifier, name, showFlags ? ent->mode : "");
                len++;
            }
            else
            {
                printf("%s%s%s\033[0m", modifier, name, showFlags ? ent->mode : "");
            }

            if (showFlags)
            {
                len += strlen(ent->mode);
            }

            if (c < numCols - 1)
            {
                for (uint64_t i = len; i < colWidth; i++)
                {
                    putchar(' ');
                }
            }
        }
        printf("\n");
    }

    free(entries);
    return 0;
}

int main(int argc, char** argv)
{
    int i = 1;
    for (; i < argc; i++)
    {
        if (argv[i][0] != '-')
        {
            break;
        }

        for (int j = 1; argv[i][j] != '\0'; j++)
        {
            if (argv[i][j] == 'a')
            {
                showAll = true;
            }
            else if (argv[i][j] == 'f')
            {
                showFlags = true;
            }
            else
            {
                fprintf(stderr, "ls: invalid option -- '%c'\n", argv[i][j]);
                return EXIT_FAILURE;
            }
        }
    }

    if (i >= argc)
    {
        if (print_dir(".") == _FAIL)
        {
            return EXIT_FAILURE;
        }
    }
    else
    {
        for (; i < argc; i++)
        {
            if (print_dir(argv[i]) == _FAIL)
            {
                return EXIT_FAILURE;
            }
        }
    }

    return EXIT_SUCCESS;
}
