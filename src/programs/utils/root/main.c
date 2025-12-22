#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/io.h>
#include <sys/proc.h>

uint64_t adjust_path(const char* command, char* out)
{
    if (strchr(command, '/') != NULL)
    {
        strncpy(out, command, MAX_PATH);
        out[MAX_PATH - 1] = '\0';
        return 0;
    }

    char* pathEnv = sreadfile("/proc/self/env/PATH");
    if (pathEnv == NULL)
    {
        pathEnv = strdup("/bin:/usr/bin");
        if (pathEnv == NULL)
        {
            return ERR;
        }
    }

    char* token = strtok(pathEnv, ":");
    while (token != NULL)
    {
        if (snprintf(out, MAX_PATH, "%s/%s", token, command) < MAX_PATH)
        {
            stat_t info;
            if (stat(out, &info) != ERR && info.type != INODE_DIR)
            {
                free(pathEnv);
                return 0;
            }
        }
        token = strtok(NULL, ":");
    }

    free(pathEnv);
    return ERR;
}

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        printf("%s <command> [args...]\n", argv[0]);
        return EXIT_FAILURE;
    }

    int result = EXIT_SUCCESS;
    char* id = NULL;
    fd_t ctl = ERR;
    fd_t data = ERR;
    fd_t ns = ERR;
    char** childArgv = NULL;
    char* status = NULL;

    id = sreadfile("/net/local/seqpacket");
    if (id == NULL)
    {
        printf("%s: failed to open local seqpacket socket (%s)\n", argv[0], strerror(errno));
        result = EXIT_FAILURE;
        goto cleanup;
    }

    ctl = open(F("/net/local/%s/ctl", id));
    if (ctl == ERR)
    {
        printf("%s: failed to open ctl socket (%s)\n", argv[0], strerror(errno));
        result = EXIT_FAILURE;
        goto cleanup;
    }

    data = open(F("/net/local/%s/data", id));
    if (data == ERR)
    {
        printf("%s: failed to open data socket (%s)\n", argv[0], strerror(errno));
        result = EXIT_FAILURE;
        goto cleanup;
    }

    if (swrite(ctl, "connect root") == ERR)
    {
        printf("%s: failed to connect to root (%s)\n", argv[0], strerror(errno));
        result = EXIT_FAILURE;
        goto cleanup;
    }

    if (swrite(data, "Hello from client!") == ERR)
    {
        printf("%s: failed to send message to root (%s)\n", argv[0], strerror(errno));
        result = EXIT_FAILURE;
        goto cleanup;
    }

    key_t key = {0};
    if (read(data, &key, sizeof(key)) != sizeof(key))
    {
        printf("%s: failed to read key from root (%s)\n", argv[0], strerror(errno));
        result = EXIT_FAILURE;
        goto cleanup;
    }

    ns = claim(&key);
    if (ns == ERR)
    {
        printf("%s: failed to claim key from root (%s)\n", argv[0], strerror(errno));
        result = EXIT_FAILURE;
        goto cleanup;
    }

    childArgv = calloc(argc, sizeof(char*));
    if (childArgv == NULL)
    {
        printf("%s: failed to allocate memory for child argv (%s)\n", argv[0], strerror(errno));
        result = EXIT_FAILURE;
        goto cleanup;
    }

    char path[MAX_PATH];
    if (adjust_path(argv[1], path) == ERR)
    {
        printf("%s: failed to adjust path for %s (%s)\n", argv[0], argv[1], strerror(errno));
        result = EXIT_FAILURE;
        goto cleanup;
    }

    childArgv[0] = path;
    for (int i = 2; i < argc; i++)
    {
        childArgv[i - 1] = argv[i];
    }
    childArgv[argc - 1] = NULL;

    pid_t child = spawn((const char**)childArgv, SPAWN_SUSPEND);
    if (child == ERR)
    {
        printf("%s: failed to spawn child (%s)\n", argv[0], strerror(errno));
        result = EXIT_FAILURE;
        goto cleanup;
    }

    if (swritefile(F("/proc/%d/ctl", child), F("join %llu && start", ns)) == ERR)
    {
        printf("%s: failed to join child to namespace (%s)\n", argv[0], strerror(errno));
        result = EXIT_FAILURE;
        goto cleanup;
    }

    status = sreadfile(F("/proc/%d/wait", child));
    if (status == NULL)
    {
        printf("%s: failed to read child status (%s)\n", argv[0], strerror(errno));
        result = EXIT_FAILURE;
        goto cleanup;
    }

cleanup:
    free(childArgv);
    close(ns);
    close(data);
    close(ctl);
    free(id);
    if (status != NULL)
    {
        _exit(status);
    }
    return result;
}