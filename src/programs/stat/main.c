#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/io.h>
#include <time.h>

static const char* type_to_string(inode_type_t type)
{
    switch (type)
    {
    case INODE_FILE:
        return "File";
    case INODE_DIR:
        return "Directory";
    default:
        return "Unknown";
    }
}

static void print_stat(const char* path)
{
    stat_t st;
    if (stat(path, &st) == ERR)
    {
        printf("stat: failed to st %s (%s)\n", path, strerror(errno));
        return;
    }

    printf("  File: %s\n", path);
    printf("  Name: %s\n", st.name);
    printf("Number: %llu\n", st.number);
    printf("  Type: %s\n", type_to_string(st.type));
    printf("  Size: %llu\n", st.size);
    printf("Blocks: %llu\n", st.blocks);
    printf(" Links: %llu\n", st.linkAmount);

    struct tm timeData;
    char buffer[MAX_PATH];
    localtime_r(&st.accessTime, &timeData);
    printf("Access: %02d:%02d %d-%02d-%02d\n", timeData.tm_hour, timeData.tm_min, timeData.tm_year + 1900,
        timeData.tm_mon + 1, timeData.tm_mday);
    localtime_r(&st.modifyTime, &timeData);
    printf("Modify: %02d:%02d %d-%02d-%02d\n", timeData.tm_hour, timeData.tm_min, timeData.tm_year + 1900,
        timeData.tm_mon + 1, timeData.tm_mday);
    localtime_r(&st.changeTime, &timeData);
    printf("Change: %02d:%02d %d-%02d-%02d\n", timeData.tm_hour, timeData.tm_min, timeData.tm_year + 1900,
        timeData.tm_mon + 1, timeData.tm_mday);
    localtime_r(&st.createTime, &timeData);
    printf("Create: %02d:%02d %d-%02d-%02d\n", timeData.tm_hour, timeData.tm_min, timeData.tm_year + 1900,
        timeData.tm_mon + 1, timeData.tm_mday);
}

int main(int argc, char** argv)
{
    for (int i = 1; i < argc; i++)
    {
        print_stat(argv[i]);
    }

    return EXIT_SUCCESS;
}
