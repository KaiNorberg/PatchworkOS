#include <stdint.h>
#include <lib-process.h>

int main()
{   
    while (1)
    {
        sys_test("Hello from sleep_test, sleeping...");
        
        sleep(1000);        
    }

    return 0;
}