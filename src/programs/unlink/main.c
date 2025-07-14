#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/io.h>

int main(int argc, char** argv)
{
    for (int i = 1; i < argc; i++)
    {
        if (unlink(argv[i]) == ERR)
        {
            fprintf(stderr, "unlink: %s\n", strerror(errno));
            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}
