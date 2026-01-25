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
        fprintf(stderr, "%s <target> <linkpath>\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (symlink(argv[1], argv[2]) == _FAIL)
    {
        fprintf(stderr, "symlink: failed to create symlink %s -> %s (%s)\n", argv[2], argv[1], strerror(errno));
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
