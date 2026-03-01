#include <_libstd/clock_t.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fs.h>
#include <sys/io.h>

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

    fd_t file;
    if (IS_ERR(open(&file, filename)))
    {
        fprintf(stderr, "tail: cannot open file '%s'\n", filename);
        return EXIT_FAILURE;
    }

    while (true)
    {
        iopoll_t revents;
        iopoll(file, IOEVENT_READ, follow ? CLOCKS_NEVER : CLOCKS_NOW, &revents);
        if (revents & IOEVENT_READ)
        {
            char buffer[1024];
            uint64_t bytesRead;
            status_t status = ioread(file, buffer, sizeof(buffer), IOCUR, &bytesRead);
            if (bytesRead > 0)
            {
                iowrite(STDOUT_FILENO, buffer, bytesRead, IOCUR, NULL);
            }
        }
        else if (!follow)
        {
            break;
        }
    }

    return 0;
}