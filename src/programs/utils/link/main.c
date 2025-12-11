#define __STDC_WANT_LIB_EXT1__ 1
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/io.h>

int main(int argc, char** argv)
{
    if (argc != 3)
    {
        fprintf(stderr, "%s <source> <destination>\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (link(argv[1], argv[2]) == ERR)
    {
        fprintf(stderr, "link: failed to create link (%s)\n", strerror(errno));
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
