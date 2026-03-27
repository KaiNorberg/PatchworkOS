#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libstd/fs.h>
#include <libstd/status.h>

int main(int argc, char** argv)
{
    for (int i = 1; i < argc; i++)
    {
        fd_t fd;
        if (IS_ERR(open(&fd, argv[i])))
        {
            printf("open: failed to open %s (%s)\n", argv[i], strerror(errno));
            return EXIT_FAILURE;
        }

        close(fd);
    }

    return EXIT_SUCCESS;
}
