#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>
#include <sys/io.h>
#include <sys/process.h>

#include <libs/std/internal/syscalls/syscalls.h>

#define BUFFER_SIZE 32

int main(void)
{
    while (1)
    {
        fd_t fd = open("/test1/test2/test3/test.txt");
        if (fd == ERR)
        {
            SYSCALL(SYS_TEST, 1, strerror(errno));
            return EXIT_FAILURE;
        }

        char buffer[BUFFER_SIZE];
        memset(buffer, 0, BUFFER_SIZE);    
        if (read(fd, buffer, BUFFER_SIZE - 1) == ERR)
        {
            SYSCALL(SYS_TEST, 1, strerror(errno));
            return EXIT_FAILURE;
        }

        if (close(fd) == ERR)
        {
            SYSCALL(SYS_TEST, 1, strerror(errno));
            return EXIT_FAILURE;
        }

        SYSCALL(SYS_TEST, 1, buffer);
    
        /*struct timespec duration;
        duration.tv_sec = 1;
        duration.tv_nsec = 0;
        thrd_sleep(&duration, NULL);*/
    }

    return EXIT_SUCCESS;
}
