#include <stdlib.h>
#include <string.h>
#include <sys/io.h>
#include <sys/proc.h>

int main(void)
{
    /*fd_t client = socket(DOMAIN_LOCAL, SOCK_STREAM, PROTO_DEFAULT);
    if (client == ERR) 
    {
        return EXIT_FAILURE;
    }

    sockaddr_t serverAddr;
    strcpy(serverAddr.local.name, "test");

    if (connect(client, &serverAddr) == ERR) 
    {
        close(client);
        return EXIT_FAILURE;
    }

    const char* message = "Hello from the client!";
    if (write(client, message, strlen(message) + 1) == ERR) 
    {
        close(client);
        return EXIT_FAILURE;
    }

    close(client);*/

    return EXIT_SUCCESS;
}