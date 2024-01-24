#include <stdint.h>
#include <lib-process.h>

int main()
{       
    uint64_t pid = fork();

    if (pid == 0)
    {
        while (1)
        {
            sys_test("Hello from child!     \r");
        }         
    }
    else
    {
        while (1)
        {
            sys_test("Hello from parent!    \r");
        }       
    }

    return 0;
}
