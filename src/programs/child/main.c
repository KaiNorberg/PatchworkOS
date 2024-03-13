#include <stdint.h>

#include <lib-asym.h>

#include <libc/string.h>

#define BUFFER_SIZE 32

char buffer[BUFFER_SIZE];

int main(void)
{       
    while (1)
    {
        int64_t fd = open("ram:/test/test.txt", FILE_FLAG_READ);
        if (fd == -1)
        {
            sys_test(status_string());
            exit(1);
        }
        
        memset(buffer, 0, BUFFER_SIZE);
        if (read(fd, buffer, BUFFER_SIZE - 1) == -1)
        {
            sys_test(status_string());
            exit(1);
        }

        if (close(fd) == -1)
        {
            sys_test(status_string());
            exit(1);
        }

        sys_test(buffer);
    }
    
    return 0;
}
