#include <stdint.h>
#include <lib-process.h>

int main()
{   
    while (1)
    {
        sys_test("Hello from test program!              ");
    }

    return 0;
}