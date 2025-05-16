#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/io.h>

int main(int argc, char** argv)
{
    for (int i = 1; i < argc; i++)
    {
        if (remove(argv[i]) == EOF)
        {
            fprintf(stderr, "rm: %s\n", strerror(errno));
            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}