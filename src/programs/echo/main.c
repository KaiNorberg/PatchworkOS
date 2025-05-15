#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/io.h>

// TODO: Fix this very basic and wrong echo implementation
int main(int argc, char** argv)
{
    for (int i = 1; i < argc; i++)
    {
        // Important to use write not stdio for actions, for example "kill > sys:/proc/*/ctl".
        if (writef(STDOUT_FILENO, argv[i]) == ERR)
        {
            fprintf(stderr, "echo: %s\n", strerror(errno));
            return EXIT_FAILURE;
        }

        if (i != argc - 1)
        {
            if (writef(STDOUT_FILENO, " ") == ERR)
            {
                fprintf(stderr, "echo: %s\n", strerror(errno));
                return EXIT_FAILURE;
            }
        }
    }
    return EXIT_SUCCESS;
}
