#include <stdint.h>
#include <lib-process.h>

int main()
{   
    sys_test("Hello from parent, forking...     ");
    
    //nanosleep(NANOSECONDS_PER_SECOND);

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
