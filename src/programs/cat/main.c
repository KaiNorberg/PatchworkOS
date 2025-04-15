#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/io.h>

#define BUFFER_SIZE 1024

int main(int argc, char** argv)
{
    if (argc == 1)
    {
        while (1)
        {
            char buffer[BUFFER_SIZE];
            uint64_t count = read(STDIN_FILENO, buffer, BUFFER_SIZE);
            if (count == ERR)
            {
                printf("error: reading stdin (%s)\n", strerror(errno));
                return EXIT_FAILURE;
            }
            if (count == 0)
            {
                break;
            }

            writef(STDOUT_FILENO, buffer);
        }
    }

    for (int i = 1; i < argc; i++)
    {
        fd_t fd = open(argv[i]);
        if (fd == ERR)
        {
            printf("error: can't open %s (%s)\n", argv[i], strerror(errno));
            continue;
        }

        while (1)
        {
            char buffer[BUFFER_SIZE];
            uint64_t count = read(fd, buffer, BUFFER_SIZE);
            if (count == ERR)
            {
                printf("error: reading %s (%s)\n", argv[i], strerror(errno));
                close(fd);
                return EXIT_FAILURE;
            }
            if (count == 0)
            {
                break;
            }

            writef(STDOUT_FILENO, buffer);
        }

        close(fd);
    }

    return EXIT_SUCCESS;
}
