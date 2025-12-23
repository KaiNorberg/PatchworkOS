#include "root.h"

#include <_internal/MAX_PATH.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/io.h>

#define ROOT_PASSWORD "1234"

uint64_t root_handle_client(fd_t client)
{
    char buffer[MAX_PATH] = {0};
    if (read(client, buffer, sizeof(buffer) - 1) == ERR)
    {
        return ERR;
    }

    printf("root: received password attempt: %s\n", buffer);
    if (strcmp(buffer, ROOT_PASSWORD) != 0)
    {
        errno = EPERM;
        return ERR;
    }

    fd_t ns = open("/proc/self/ns");
    if (ns == ERR)
    {
        return ERR;
    }

    key_t key;
    if (share(&key, ns, CLOCKS_NEVER) == ERR)
    {
        close(ns);
        return ERR;
    }

    write(client, &key, sizeof(key));
    close(ns);
    return 0;
}

void root_start(void)
{
    char* id = sreadfile("/net/local/seqpacket");
    if (id == NULL)
    {
        printf("root: failed to open local seqpacket socket (%s)\n", strerror(errno));
        abort();
    }

    if (swritefile(F("/net/local/%s/ctl", id), "bind root && listen") == ERR)
    {
        printf("root: failed to bind to root (%s)\n", strerror(errno));
        goto error;
    }

    printf("root: listening for connections...\n");
    while (1)
    {
        fd_t client = open(F("/net/local/%s/accept", id));
        if (client == ERR)
        {
            printf("init: failed to accept connection (%s)\n", strerror(errno));
            goto error;
        }

        printf("root: accepted connection\n");

        if (root_handle_client(client) == ERR)
        {
            printf("root: failed to handle client (%s)\n", strerror(errno));
        }

        close(client);
    }

error:
    free(id);
    abort();
}