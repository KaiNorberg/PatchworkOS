#include <errno.h>
#include <kernel/ipc/note.h>
#include <stdio.h>
#include <string.h>
#include <sys/fs.h>
#include <sys/proc.h>
#include <sys/status.h>
#include <time.h>
#include <patchwork/patchwork.h>

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

    status_t status = notify(note_handler);
    if (IS_ERR(status))
    {
        printf("boxspawn: failed to register note handler (%s)\n", codetostr(ST_CODE(status)));
        return EXIT_FAILURE;
    }

    char* id;
    status = readfiles(&id, "/net/local/seqpacket");
    if (IS_ERR(status))
    {
        printf("boxspawn: failed to open local seqpacket socket (%s)\n", codetostr(ST_CODE(status)));
        return EXIT_FAILURE;
    }

    status = writefiles(F("/net/local/%s/ctl", id), "connect boxspawn");
    if (IS_ERR(status))
    {
        printf("boxspawn: failed to connect to boxspawn (%s)\n", codetostr(ST_CODE(status)));
        free(id);
        return EXIT_FAILURE;
    }

    char stdio[3][KEY_128BIT];
    for (uint8_t i = 0; i < 3; i++)
    {
        status = share(stdio[i], sizeof(stdio[i]), i, CLOCKS_PER_SEC);
        if (IS_ERR(status))
        {
            printf("boxspawn: failed to share stdio (%s)\n", codetostr(ST_CODE(status)));
            free(id);
            return EXIT_FAILURE;
        }
    }

    char group[KEY_128BIT] = {0};
    char namespace[KEY_128BIT] = {0};
    status = sharefile(group, sizeof(group), "/proc/self/group", CLOCKS_PER_SEC);
    if (IS_ERR(status))
    {
        if (ST_CODE(status) != ST_CODE_NOENT)
        {
            printf("boxspawn: failed to share group (%s)\n", codetostr(ST_CODE(status)));
            free(id);
            return EXIT_FAILURE;
        }

        printf("boxspawn: `/proc` does not appear to be mounted, foreground boxes will not work correctly\n");
    }
    else
    {
        status = sharefile(namespace, sizeof(namespace), "/proc/self/ns", CLOCKS_PER_SEC);
        if (IS_ERR(status))
        {
            printf("boxspawn: failed to share namespace (%s)\n", codetostr(ST_CODE(status)));
            free(id);
            return EXIT_FAILURE;
        }
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
            printf("boxspawn: arguments too long\n");
            free(id);
            return EXIT_FAILURE;
        }
        strcat(buffer, " ");
        strcat(buffer, argv[i]);
    }

    fd_t data;
    status = open(&data, F("/net/local/%s/data", id));
    if (IS_ERR(status))
    {
        printf("boxspawn: failed to open data socket (%s)\n", codetostr(ST_CODE(status)));
        free(id);
        return EXIT_FAILURE;
    }

    status = writes(data, buffer, NULL);
    if (IS_ERR(status))
    {
        printf("boxspawn: failed to send request (%s)\n", codetostr(ST_CODE(status)));
        free(id);
        close(data);
        return EXIT_FAILURE;
    }

    memset(buffer, 0, sizeof(buffer));

    status = read(data, buffer, sizeof(buffer) - 1, NULL);
    if (IS_ERR(status))
    {
        printf("boxspawn: failed to read response (%s)\n", codetostr(ST_CODE(status)));
        free(id);
        close(data);
        return EXIT_FAILURE;
    }
    close(data);

    if (wordcmp(buffer, "error") == 0)
    {
        printf("boxspawn: %s\n", buffer);
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
        printf("boxspawn: failed to parse response (%s)\n", strerror(errno));
        free(id);
        return EXIT_FAILURE;
    }

    fd_t wait;
    status = claim(&wait, waitkey);
    if (IS_ERR(status))
    {
        printf("boxspawn: failed to claim response (%s)\n", codetostr(ST_CODE(status)));
        free(id);
        return EXIT_FAILURE;
    }

    char string[NOTE_MAX];
    status = RETRY_ON_CODE(read(wait, string, sizeof(string) - 1, NULL), INTR);
    if (IS_ERR(status))
    {
        printf("boxspawn: failed to read status (%s)\n", codetostr(ST_CODE(status)));
        free(id);
        close(wait);
        return EXIT_FAILURE;
    }
    close(wait);

    exits(string);
}