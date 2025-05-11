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
        writef(STDOUT_FILENO, argv[i]); // Important to use write not stdio for actions, for example "kill > sys:/proc/*/ctl".
        if (i != argc - 1)
        {
            writef(STDOUT_FILENO, " ");
        }
    }
    return EXIT_SUCCESS;
}
