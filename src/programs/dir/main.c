#define __STDC_WANT_LIB_EXT1__ 1
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/io.h>

static uint64_t print_dir(const char* path)
{
    fd_t fd = openf("%s:dir", path);
    if (fd == ERR)
    {
        fprintf(stderr, "dir: can't open directory %s (%s)\n", path, strerror(errno));
        return ERR;
    }

    dirent_t* entries = NULL;
    uint64_t entryCount = 0;
    uint64_t bufferSize = 64;
    entries = (dirent_t*)malloc(sizeof(dirent_t) * bufferSize);
    if (entries == NULL)
    {
        close(fd);
        fprintf(stderr, "dir: memory allocation failed\n");
        return ERR;
    }

    uint64_t bytesRead;
    while ((bytesRead = getdents(fd, &entries[entryCount], sizeof(dirent_t) * (bufferSize - entryCount))) > 0)
    {
        if (bytesRead == ERR)
        {
            close(fd);
            free(entries);
            fprintf(stderr, "dir: can't read directory %s (%s)\n", path, strerror(errno));
            return ERR;
        }
        entryCount += bytesRead / sizeof(dirent_t);
        if (entryCount >= bufferSize)
        {
            bufferSize *= 2;
            dirent_t* newEntries = (dirent_t*)realloc(entries, sizeof(dirent_t) * bufferSize);
            if (newEntries == NULL)
            {
                close(fd);
                free(entries);
                fprintf(stderr, "dir: re-allocation failed\n");
                return ERR;
            }
            entries = newEntries;
        }
    }
    close(fd);

    if (entryCount == 0)
    {
        free(entries);
        return 0;
    }

    uint64_t maxLength = 0;
    for (uint64_t i = 0; i < entryCount; i++)
    {
        uint64_t nameLength = strlen(entries[i].name) + (entries[i].type == INODE_DIR ? 1 : 0);
        if (nameLength > maxLength)
        {
            maxLength = nameLength;
        }
    }

    uint32_t terminalWidth = 80; // Just assume its 80 for now.
    uint32_t columnWidth = maxLength + 2;
    if (columnWidth > terminalWidth)
    {
        columnWidth = terminalWidth;
    }
    uint32_t numColumns = terminalWidth / columnWidth;
    if (numColumns == 0)
    {
        numColumns = 1;
    }

    uint32_t numRows = (entryCount + numColumns - 1) / numColumns;

    for (uint32_t row = 0; row < numRows; row++)
    {
        for (uint32_t col = 0; col < numColumns; col++)
        {
            uint64_t index = col * numRows + row;
            if (index < entryCount)
            {
                const char* name = entries[index].name;
                bool isDir = entries[index].type == INODE_DIR;
                printf("%-*s", columnWidth, isDir ? strcat(strcpy((char[MAX_PATH]){0}, name), "/") : name);
            }
        }
        printf("\n");
    }

    free(entries);
    return 0;
}

uint32_t main(uint32_t argc, char** argv)
{
    if (argc <= 1)
    {
        if (print_dir(".") == ERR)
        {
            return EXIT_FAILURE;
        }
    }
    else
    {
        for (uint32_t i = 1; i < argc; i++)
        {
            if (print_dir(argv[i]) == ERR)
            {
                return EXIT_FAILURE;
            }
        }
    }

    return EXIT_SUCCESS;
}
