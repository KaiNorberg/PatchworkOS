#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/io.h>

int main(int argc, char** argv)
{
    for (int i = 1; i < argc; i++)
    {
        if (writes(STDOUT_FILENO, argv[i]) == ERR)
        {
            fprintf(stderr, "echo: %s\n", strerror(errno));
            return EXIT_FAILURE;
        }

        if (i != argc - 1)
        {
            if (writes(STDOUT_FILENO, " ") == ERR)
            {
                fprintf(stderr, "echo: %s\n", strerror(errno));
                return EXIT_FAILURE;
            }
        }
    }
    return EXIT_SUCCESS;
}
