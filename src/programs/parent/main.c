#include <stdint.h>
#include <lib-process.h>
#include <lib-filesystem.h>
#include <lib-status.h>

int main()
{               
    //sys_test("Spawning child...");

    int64_t pid = spawn("ram:/bin/child.elf");
    if (pid == -1)
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
