#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/io.h>

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    int result = EXIT_SUCCESS;
    char* id = NULL;
    fd_t ctl = ERR;
    fd_t data = ERR;

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

cleanup:
    free(id);
    close(ctl);
    return result;
}