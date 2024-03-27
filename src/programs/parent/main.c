#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/io.h>

#include <libs/std/internal/syscalls/syscalls.h>

int main(void)
{
    fd_t fd = open("ram:/test1/test2/test3/test.txt", O_READ);
    if (fd == ERR)
    {
        exit(EXIT_FAILURE);
    }
    
    char buffer[32];
    memset(buffer, 0, 32);
    if (read(fd, buffer, 31) == ERR)
    {
        exit(EXIT_FAILURE);
    }
    
    if (close(fd) == ERR)
    {
        exit(EXIT_FAILURE);
    }
    
    while (1)
    {
        SYSCALL(SYS_TEST, 1, buffer);
    }

    return 0;
}
