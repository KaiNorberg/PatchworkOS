#include "root.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/io.h>

void root_service_start(void)
{
    /*char* id = sreadfile("/net/local/seqpacket");
    if (id == NULL)
    {
        printf("root: failed to open local seqpacket socket (%s)\n", strerror(errno));
        abort();
    }

    if (swritefile(F("/net/local/%s/ctl", id), "bind root && listen") == ERR)
    {
        printf("root: failed to bind to root (%s)\n", strerror(errno));
        free(id);
        abort();
    }

    while (true)
    {


    }*/
}