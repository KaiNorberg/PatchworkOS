#include <stdint.h>

#include "stdlib.h"

#include "../../common.h"

extern uint64_t sys_fork();

extern uint64_t sys_test(const char* string);

int main(int argc, char* argv[])
{   
    sys_test("Hello from parent, forking...\r\n\n");
    
    uint64_t pid = sys_fork();

    if (pid == 0)
    {
        while (1)
        {
            sys_test("Hello from child program!     \r");
        }            
    }
    else
    {
        while (1)
        {
            sys_test("Hello from parent program!    \r");
        }       
    }       

    return 0;
}