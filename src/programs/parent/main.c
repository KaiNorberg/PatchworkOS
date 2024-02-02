#include <stdint.h>
#include <lib-process.h>

int main()
{       
    spawn("/bin/child.elf");

    while (1)
    {
        sys_test("Hello from parent!    \r");
    }

    return 0;
}
