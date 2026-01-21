#define __STDC_WANT_LIB_EXT1__ 1
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fs.h>

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "%s <path>\n", argv[0]);
        return EXIT_FAILURE;
    }

    char buffer[MAX_PATH];
    uint64_t len = readlink(argv[1], buffer, sizeof(buffer) - 1);
    if (len == ERR)
    {
        fprintf(stderr, "readlink: failed to readlink %s (%s)\n", argv[1], strerror(errno));
        return EXIT_FAILURE;
    }
    buffer[len] = '\0';
    printf("%s\n", buffer);

    return EXIT_SUCCESS;
}
