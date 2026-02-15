#include <errno.h>
#include <kernel/ipc/note.h>
#include <patchwork/patchwork.h>
#include <stdio.h>
#include <string.h>
#include <sys/fs.h>
#include <sys/proc.h>
#include <sys/status.h>
#include <time.h>

#define BUFFER_MAX 0x1000

void note_handler(char* note)
{
    UNUSED(note);

    // Do nothing
    note_done();
}

int main(int argc, char** argv)
{
    if (argc < 1)
    {
        return EXIT_FAILURE;
    }

    status_t status = note_set(note_handler);
    if (IS_ERR(status))
    {
        proc_exit(F("boxspawn: failed to register note handler %Y", status));
    }

    char* id;
    status = readfiles(&id, "/net/local/seqpacket");
    if (IS_ERR(status))
    {
        proc_exit(F("boxspawn: failed to open local seqpacket socket %Y", status));
    }

    status = writefiles(F("/net/local/%s/ctl", id), "connect boxspawn");
    if (IS_ERR(status))
    {
        proc_exit(F("boxspawn: failed to connect to boxspawn %Y", status));
    }

    char stdio[3][KEY_128BIT];
    for (uint8_t i = 0; i < 3; i++)
    {
        status = share(stdio[i], sizeof(stdio[i]), i, CLOCKS_PER_SEC);
        if (IS_ERR(status))
        {
            proc_exit(F("boxspawn: failed to share stdio %d %Y", i, status));
        }
    }

    char group[KEY_128BIT] = {0};
    char namespace[KEY_128BIT] = {0};
    status = sharefile(group, sizeof(group), "/proc/self/group", CLOCKS_PER_SEC);
    if (IS_ERR(status))
    {
        if (ST_CODE(status) != ST_CODE_NOENT)
        {
            proc_exit(F("boxspawn: failed to share group %Y", status));
        }

        printf("boxspawn: `/proc` does not appear to be mounted, foreground boxes will not work correctly\n");
    }
    else
    {
        status = sharefile(namespace, sizeof(namespace), "/proc/self/ns", CLOCKS_PER_SEC);
        if (IS_ERR(status))
        {
            proc_exit(F("boxspawn: failed to share namespace %Y", status));
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
            proc_exit("boxspawn: arguments too long");
        }
        strcat(buffer, " ");
        strcat(buffer, argv[i]);
    }

    fd_t data;
    status = open(&data, F("/net/local/%s/data", id));
    if (IS_ERR(status))
    {
        proc_exit(F("boxspawn: failed to open data socket %Y", status));
    }

    status = writes(data, buffer, NULL);
    if (IS_ERR(status))
    {
        proc_exit(F("boxspawn: failed to send request %Y", status));
    }

    memset(buffer, 0, sizeof(buffer));

    status = ioread(data, buffer, sizeof(buffer) - 1, IOCUR, NULL);
    if (IS_ERR(status))
    {
        proc_exit(F("boxspawn: failed to ioread response %Y", status));
    }
    close(data);

    if (wordcmp(buffer, "error") == 0)
    {
        proc_exit(F("boxspawn: %s", buffer));
    }

    if (wordcmp(buffer, "background") == 0)
    {
        free(id);
        return 0;
    }

    char waitkey[KEY_MAX];
    if (sscanf(buffer, "foreground %s", waitkey) != 1)
    {
        proc_exit(F("boxspawn: failed to parse response (%s)", strerror(errno)));
    }

    fd_t wait;
    status = claim(&wait, waitkey);
    if (IS_ERR(status))
    {
        proc_exit(F("boxspawn: failed to claim response %Y", status));
    }

    char string[NOTE_MAX];
    status = RETRY_ON_CODE(ioread(wait, string, sizeof(string) - 1, IOCUR, NULL), INTR);
    if (IS_ERR(status))
    {
        proc_exit(F("boxspawn: failed to ioread status %Y", status));
    }
    close(wait);

    proc_exit(string);
}