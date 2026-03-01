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
    if (argc != 3)
    {
        fprintf(stderr, "%s <source> <destination>\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (IS_ERR(link(argv[1], argv[2])))
    {
        fprintf(stderr, "link: failed to create link (%s)\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
