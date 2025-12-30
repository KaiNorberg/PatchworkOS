#include <errno.h>
#include <kernel/ipc/note.h>
#include <stdio.h>
#include <string.h>
#include <sys/io.h>
#include <sys/proc.h>
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

    fd_t data = open(F("/net/local/%s/data", id));
    if (data == ERR)
    {
        printf("pkg-spawn: failed to open data socket (%s)\n", strerror(errno));
        free(id);
        return EXIT_FAILURE;
    }

    if (swrite(data, buffer) == ERR)
    {
        printf("pkg-spawn: failed to send request (%s)\n", strerror(errno));
        free(id);
        close(data);
        return EXIT_FAILURE;
    }

    memset(buffer, 0, sizeof(buffer));

    if (read(data, buffer, sizeof(buffer) - 1) == ERR)
    {
        printf("pkg-spawn: failed to read response (%s)\n", strerror(errno));
        free(id);
        close(data);
        return EXIT_FAILURE;
    }
    close(data);

    if (wordcmp(buffer, "error") == 0)
    {
        printf("pkg-spawn: %s\n", buffer);
        free(id);
        return EXIT_FAILURE;
    }

    if (wordcmp(buffer, "background") == 0)
    {
        free(id);
        return 0;
    }

    char waitkey[KEY_MAX];
    if (sscanf(buffer, "foreground %s", waitkey) != 1)
    {
        printf("pkg-spawn: failed to parse response (%s)\n", strerror(errno));
        free(id);
        return EXIT_FAILURE;
    }

    fd_t wait = claim(waitkey);
    if (wait == ERR)
    {
        printf("pkg-spawn: failed to claim response (%s)\n", strerror(errno));
        free(id);
        return EXIT_FAILURE;
    }

    char status[NOTE_MAX];
    if (read(wait, status, sizeof(status) - 1) == ERR)
    {
        printf("pkg-spawn: failed to read status (%s)\n", strerror(errno));
        free(id);
        close(wait);
        return EXIT_FAILURE;
    }
    close(wait);

    _exit(status);
}