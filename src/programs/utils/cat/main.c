#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fs.h>

#define BUFFER_SIZE 1024

static int read_fd(fd_t fd, const char* name, bool hexOutput)
{
    while (1)
    {
        char buffer[BUFFER_SIZE];
        uint64_t count;
        status_t status = read(fd, buffer, BUFFER_SIZE - 1, &count);
        if (IS_ERR(status))
        {
            printf("cat: failed to read %s (%s)\n", name, strerror(errno));
            close(fd);
            return EOF;
        }
        if (count == 0)
        {
            break;
        }

        if (hexOutput)
        {
            for (uint64_t i = 0; i < count; i++)
            {
                writes(STDOUT_FILENO, F("%02x ", (unsigned char)buffer[i]), NULL);
            }
            continue;
        }

        write(STDOUT_FILENO, buffer, count, NULL);
    }

    return 0;
}

int main(int argc, char** argv)
{
    if (argc == 1)
    {
        return read_fd(STDIN_FILENO, "stdin", false);
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
        fd_t fd;
        if (IS_ERR(open(&fd, argv[i])))
        {
            printf("cat: failed to open %s (%s)\n", argv[i], strerror(errno));
            return EXIT_FAILURE;
        }

        if (read_fd(fd, argv[i], hexOutput) == EOF)
        {
            return EXIT_FAILURE;
        }

        close(fd);
    }

    return EXIT_SUCCESS;
}
