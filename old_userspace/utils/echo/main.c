#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fs.h>

int main(int argc, char** argv)
{
    for (int i = 1; i < argc; i++)
    {
        if (IS_ERR(writes(STDOUT_FILENO, argv[i], NULL)))
        {
            fprintf(stderr, "echo: %s\n", strerror(errno));
            return EXIT_FAILURE;
        }

        if (i != argc - 1)
        {
            if (IS_ERR(writes(STDOUT_FILENO, " ", NULL)))
            {
                fprintf(stderr, "echo: %s\n", strerror(errno));
                return EXIT_FAILURE;
            }
        }
    }
    return EXIT_SUCCESS;
}
