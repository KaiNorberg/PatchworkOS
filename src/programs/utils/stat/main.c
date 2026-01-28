#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fs.h>
#include <time.h>

static const char* type_to_string(vtype_t type)
{
    switch (type)
    {
    case VREG:
        return "file";
    case VDIR:
        return "directory";
    case VSYMLINK:
        return "symlink";
    default:
        return "unknown";
    }
}

static void print_stat(const char* path)
{
    stat_t buffer;
    if (IS_ERR(stat(path, &buffer)))
    {
        printf("stat: failed to stat %s\n", path);
        return;
    }

    printf("  File: %s\n", path);
    printf("  Size: %llu\t\tBlocks: %llu\t   IO Block: %llu  %s\n", buffer.size, buffer.blocks, buffer.blockSize,
        type_to_string(buffer.type));
    printf("Superblock: %llu\tVnode: %llu\tLinks: %llu\n", buffer.sbid, buffer.number, buffer.linkAmount);
    printf("   Max: %llu\n", buffer.maxFileSize);
    printf("  Name: %s\n", buffer.name);
    printf("Access: %s", ctime(&buffer.accessTime));
    printf("Modify: %s", ctime(&buffer.modifyTime));
    printf("Change: %s", ctime(&buffer.changeTime));
    printf(" Birth: %s", ctime(&buffer.createTime));
}

int main(int argc, char** argv)
{
    for (int i = 1; i < argc; i++)
    {
        print_stat(argv[i]);
    }

    return EXIT_SUCCESS;
}
