#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/io.h>

#define BUFFER_SIZE 1024

static int cat(fd_t fd, const char* name)
{
    while (1)
    {
        char buffer[BUFFER_SIZE];
        uint64_t count = read(fd, buffer, BUFFER_SIZE - 1);
        if (count == ERR)
        {
            printf("cat: failed to read %s (%s)\n", name, strerror(errno));
            close(fd);
            return EXIT_FAILURE;
        }
        if (count == 0)
        {
            break;
        }

        write(STDOUT_FILENO, buffer, count);
    }

    return EXIT_SUCCESS;
}

int main(int argc, char** argv)
{
    if (argc == 1)
    {
        return cat(STDIN_FILENO, "stdin");
    }

    for (int i = 1; i < argc; i++)
    {
        fd_t fd = open(argv[i]);
        if (fd == ERR)
        {
            printf("cat: failed to open %s (%s)\n", argv[i], strerror(errno));
            continue;
        }

        if (cat(fd, argv[i]) == EXIT_FAILURE)
        {
            return EXIT_FAILURE;
        }

        close(fd);
    }

    return EXIT_SUCCESS;
}