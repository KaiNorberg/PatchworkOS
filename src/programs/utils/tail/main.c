#include <_libstd/clock_t.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fs.h>

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "%s [-f] <file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    bool follow = false;
    uint32_t numLines = 10;
    const char* filename = NULL;
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-f") == 0)
        {
            follow = true;
        }
        else
        {
            if (i != argc - 1)
            {
                fprintf(stderr, "tail: extra operand '%s'\n", argv[i]);
                return EXIT_FAILURE;
            }
            filename = argv[i];
        }
    }

    if (filename == NULL)
    {
        fprintf(stderr, "tail: no file specified\n");
        return EXIT_FAILURE;
    }

    fd_t file = open(filename);
    if (file == _FAIL)
    {
        fprintf(stderr, "tail: cannot open file '%s'\n", filename);
        return EXIT_FAILURE;
    }

    while (true)
    {
        if (poll1(file, POLLIN, follow ? CLOCKS_NEVER : 0) != 0)
        {
            char buffer[1024];
            uint64_t bytesRead = read(file, buffer, sizeof(buffer));
            if (bytesRead > 0)
            {
                write(STDOUT_FILENO, buffer, bytesRead);
            }
        }
        else if (!follow)
        {
            break;
        }
    }

    return 0;
}