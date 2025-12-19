#include "root.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/io.h>


void root_service_start(void)
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

    while (1)
    {
        fd_t client = open(F("/net/local/%s/accept", id));
        if (client == ERR)
        {
            printf("init: failed to accept connection (%s)\n", strerror(errno));
            goto error;
        }

        char* request = sread(client);
        if (request == NULL)
        {
            printf("root: failed to read request (%s)\n", strerror(errno));
            close(client);
            continue;
        }

        printf("root: received request: %s\n", request);
        free(request);
        close(client);
    }

error:
    free(id);
    abort();
}