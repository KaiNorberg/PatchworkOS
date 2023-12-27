#include <stdint.h>

#include "stdlib.h"

#include "../../common.h"

extern uint64_t sys_fork();

extern uint64_t sys_test(const char* string);

int main(int argc, char* argv[])
{   
    while (1)
    {
        sys_test("Hello from test program!      \r");
    }            

    return 0;
}