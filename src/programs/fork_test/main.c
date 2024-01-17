#include <stdint.h>
#include <patch-process.h>

int main()
{   
    sys_test("Hello from parent, forking...     ");
    
    uint64_t pid = fork();

    if (pid == 0)
    {
        while (1)
        {
            sys_test("Hello from child program!             ");
        }         
    }
    else
    {
        while (1)
        {
            sys_test("Hello from parent program!            ");
        }       
    }

    return 0;
}
