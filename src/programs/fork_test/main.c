#include <stdint.h>
#include <lib-process.h>

int main()
{   
    sys_test("Hello from parent, forking...");
    
    uint64_t pid = fork();

    if (pid == 0)
    {
        while (1)
        {
            sys_test("Hello from child!");
        }         
    }
    else
    {
        while (1)
        {
            sys_test("Hello from parent!");
        }       
    }

    return 0;
}
