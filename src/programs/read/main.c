#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/io.h>

#define BUFFER_SIZE 1024

static uint64_t read_fd(fd_t fd, const char* name, bool hexOutput)
{
    while (1)
    {
        char buffer[BUFFER_SIZE];
        uint64_t count = read(fd, buffer, BUFFER_SIZE - 1);
        if (count == ERR)
        {
            printf("read: failed to read %s (%s)\n", name, strerror(errno));
            close(fd);
            return ERR;
        }
        if (count == 0)
        {
            break;
        }

        if (hexOutput)
        {
            for (uint64_t i = 0; i < count; i++)
            {
                writef(STDOUT_FILENO, "%02x ", (unsigned char)buffer[i]);
            }
            continue;
        }

        write(STDOUT_FILENO, buffer, count);
    }

    return 0;
}

int main(int argc, char** argv)
{
    if (argc == 1)
    {
        return read_fd(STDIN_FILENO, "stdin");
    }

    bool hexOutput = false;
    int i = 1;
    if (strcmp(argv[1], "-hex") == 0)
    {
        hexOutput = true;
        i++;
    }

    for (; i < argc; i++)
    {
        fd_t fd = open(argv[i]);
        if (fd == ERR)
        {
            printf("read: failed to open %s (%s)\n", argv[i], strerror(errno));
            return EXIT_FAILURE;
        }

        if (read_fd(fd, argv[i], hexOutput) == ERR)
        {
            return EXIT_FAILURE;
        }

        close(fd);
    }

    return EXIT_SUCCESS;
}
