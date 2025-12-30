#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/io.h>
#include <time.h>

#define BUFFER_MAX 0x1000

int main(int argc, char** argv)
{
    if (argc < 1)
    {
        return EXIT_FAILURE;
    }

    char* id = sreadfile("/net/local/seqpacket");
    if (id == NULL)
    {
        printf("pkg-spawn: failed to open local seqpacket socket (%s)\n", strerror(errno));
        return EXIT_FAILURE;
    }

    if (swritefile(F("/net/local/%s/ctl", id), "connect pkg-spawn") == ERR)
    {
        printf("pkg-spawn: failed to bind to pkg (%s)\n", strerror(errno));
        free(id);
        return EXIT_FAILURE;
    }

    char stdio[3][KEY_128BIT];
    for (uint8_t i = 0; i < 3; i++)
    {
        if (share(stdio[i], sizeof(stdio[i]), i, CLOCKS_PER_SEC) == ERR)
        {
            printf("pkg-spawn: failed to share stdio (%s)\n", strerror(errno));
            free(id);
            return EXIT_FAILURE;
        }
    }

    char buffer[BUFFER_MAX];
    snprintf(buffer, sizeof(buffer), "stdin=%s stdout=%s stderr=%s -- ", stdio[STDIN_FILENO], stdio[STDOUT_FILENO],
        stdio[STDERR_FILENO]);

    const char* lastSlash = strrchr(argv[0], '/');
    if (lastSlash == NULL)
    {
        strcat(buffer, argv[0]);
    }
    else
    {
        strcat(buffer, lastSlash + 1);
    }

    for (int i = 1; i < argc; i++)
    {
        if (strlen(buffer) + 1 + strlen(argv[i]) >= BUFFER_MAX)
        {
            printf("pkg-spawn: arguments too long\n");
            free(id);
            return EXIT_FAILURE;
        }
        strcat(buffer, " ");
        strcat(buffer, argv[i]);
    }

    if (swritefile(F("/net/local/%s/data", id), buffer) == ERR)
    {
        printf("pkg-spawn: failed to send request (%s)\n", strerror(errno));
        free(id);
        return EXIT_FAILURE;
    }

    free(id);
    return 0;
}