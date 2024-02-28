#include <stdint.h>

#include <lib-asym.h>

int main()
{
    int64_t pid = spawn("ram:/bin/child.elf");
    if (pid == -1)
    {
        sys_test(status_string());
        exit(STATUS_FAILURE);
    }
    
    while (1)
    {
        sys_test("Hello from parent!");
    }

    return 0;
}
