#include <errno.h>
#include <kernel/ipc/note.h>
#include <stdio.h>
#include <string.h>
#include <sys/io.h>
#include <sys/proc.h>
#include <time.h>

#define BUFFER_MAX 0x1000

void note_handler(char* note)
{
    UNUSED(note);

    // Do nothing
    noted();
}

int main(int argc, char** argv)
{
    if (argc < 1)
    {
        return EXIT_FAILURE;
    }

    if (notify(note_handler) == ERR)
    {
        printf("pkgspawn: failed to register note handler (%s)\n", strerror(errno));
        return EXIT_FAILURE;
    }

    char* id = sreadfile("/net/local/seqpacket");
    if (id == NULL)
    {
        printf("pkgspawn: failed to open local seqpacket socket (%s)\n", strerror(errno));
        return EXIT_FAILURE;
    }

    if (swritefile(F("/net/local/%s/ctl", id), "connect pkgspawn") == ERR)
    {
        printf("pkgspawn: failed to bind to pkg (%s)\n", strerror(errno));
        free(id);
        return EXIT_FAILURE;
    }

    char stdio[3][KEY_128BIT];
    for (uint8_t i = 0; i < 3; i++)
    {
        if (share(stdio[i], sizeof(stdio[i]), i, CLOCKS_PER_SEC) == ERR)
        {
            printf("pkgspawn: failed to share stdio (%s)\n", strerror(errno));
            free(id);
            return EXIT_FAILURE;
        }
    }

    char group[KEY_128BIT] = {0};
    char namespace[KEY_128BIT] = {0};
    if (sharefile(group, sizeof(group), "/proc/self/group", CLOCKS_PER_SEC) == ERR)
    {
        if (errno != ENOENT)
        {
            printf("pkgspawn: failed to share group (%s)\n", strerror(errno));
            free(id);
            return EXIT_FAILURE;
        }

        printf("pkgspawn: `/proc` does not appear to be mounted, foreground packages will not work correctly\n");
    }
    else if (sharefile(namespace, sizeof(namespace), "/proc/self/ns", CLOCKS_PER_SEC) == ERR)
    {
        printf("pkgspawn: failed to share namespace (%s)\n", strerror(errno));
        free(id);
        return EXIT_FAILURE;
    }

    char buffer[BUFFER_MAX];
    if (group[0] != '\0')
    {
        snprintf(buffer, sizeof(buffer), "group=%s namespace=%s stdin=%s stdout=%s stderr=%s -- ", group, namespace,
            stdio[STDIN_FILENO], stdio[STDOUT_FILENO], stdio[STDERR_FILENO]);
    }
    else
    {
        snprintf(buffer, sizeof(buffer), "stdin=%s stdout=%s stderr=%s -- ", stdio[STDIN_FILENO], stdio[STDOUT_FILENO],
            stdio[STDERR_FILENO]);
    }

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
            printf("pkgspawn: arguments too long\n");
            free(id);
            return EXIT_FAILURE;
        }
        strcat(buffer, " ");
        strcat(buffer, argv[i]);
    }

    fd_t data = open(F("/net/local/%s/data", id));
    if (data == ERR)
    {
        printf("pkgspawn: failed to open data socket (%s)\n", strerror(errno));
        free(id);
        return EXIT_FAILURE;
    }

    if (swrite(data, buffer) == ERR)
    {
        printf("pkgspawn: failed to send request (%s)\n", strerror(errno));
        free(id);
        close(data);
        return EXIT_FAILURE;
    }

    memset(buffer, 0, sizeof(buffer));

    if (read(data, buffer, sizeof(buffer) - 1) == ERR)
    {
        printf("pkgspawn: failed to read response (%s)\n", strerror(errno));
        free(id);
        close(data);
        return EXIT_FAILURE;
    }
    close(data);

    if (wordcmp(buffer, "error") == 0)
    {
        printf("pkgspawn: %s\n", buffer);
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
        printf("pkgspawn: failed to parse response (%s)\n", strerror(errno));
        free(id);
        return EXIT_FAILURE;
    }

    fd_t wait = claim(waitkey);
    if (wait == ERR)
    {
        printf("pkgspawn: failed to claim response (%s)\n", strerror(errno));
        free(id);
        return EXIT_FAILURE;
    }

    char status[NOTE_MAX];
    if (RETRY_EINTR(read(wait, status, sizeof(status) - 1)) == ERR)
    {
        printf("pkgspawn: failed to read status (%s)\n", strerror(errno));
        free(id);
        close(wait);
        return EXIT_FAILURE;
    }
    close(wait);

    _exit(status);
}