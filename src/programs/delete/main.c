#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/io.h>

int main(int argc, char** argv)
{
    for (int i = 1; i < argc; i++)
    {
        if (delete (argv[i]) == ERR)
        {
            fprintf(stderr, "delete: %s\n", strerror(errno));
            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}
