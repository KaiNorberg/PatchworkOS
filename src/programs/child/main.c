#include <stdint.h>
#include <lib-process.h>

int main()
{       
    while (1)
    {
        sys_test("Hello from child!");
    } 

    return 0;
}
