#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/io.h>

#define BUFFER_MAX 0x1000

int main(int argc, char** argv)
{
    if (argc != 1)
    {
        printf("usage: %s\n", argv[0]);
        return EXIT_FAILURE;
    }

    char* id = sreadfile("/net/local/seqpacket");
    if (id == NULL)
    {
        printf("pkg-run: failed to open local seqpacket socket (%s)\n", strerror(errno));
        return EXIT_FAILURE;
    }

    if (swritefile(F("/net/local/%s/ctl", id), "connect pkg") == ERR)
    {
        printf("pkg-run: failed to bind to pkg (%s)\n", strerror(errno));
        free(id);
        return EXIT_FAILURE;
    }

    char buffer[BUFFER_MAX];
    const char* lastSlash = strrchr(argv[0], '/');
    if (lastSlash == NULL)
    {
        strcpy(buffer, argv[0]);
    }
    else
    {
        strcpy(buffer, lastSlash + 1);
    }

    for (int i = 1; i < argc; i++)
    {
        if (strlen(buffer) + 1 + strlen(argv[i]) >= BUFFER_MAX)
        {
            printf("pkg-run: arguments too long\n");
            free(id);
            return EXIT_FAILURE;
        }
        strcat(buffer, " ");
        strcat(buffer, argv[i]);
    }

    if (swritefile(F("/net/local/%s/data", id), buffer) == ERR)
    {
        printf("pkg-run: failed to send request (%s)\n", strerror(errno));
        free(id);
        return EXIT_FAILURE;
    }

    free(id);
    return 0;
}