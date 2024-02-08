#include <stdint.h>
#include <lib-process.h>
#include <lib-filesystem.h>
#include <lib-status.h>

int main()
{               
    int64_t fd = open("ram:/bin/child.elf", FILE_FLAG_READ);
    if (fd == -1)
    {
        sys_test(status_string());
        exit(1);
    }

    int64_t pid = spawn(fd);
    if (pid == -1)
    {
        sys_test(status_string());
        exit(1);
    }

    if (close(fd) == -1)
    {
        sys_test(status_string());
        exit(1);
    }
    
    while (1)
    {
        sys_test("Hello from parent!");
    }

    return 0;
}
