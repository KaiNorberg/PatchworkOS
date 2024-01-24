#include <stdint.h>
#include <lib-process.h>

int main()
{   
    uint8_t tick = 0;
    while (1)
    {
        if (tick == 0)
        {
            sys_test("Hello from sleep_test, tick = 0\n\r");
        }
        else
        {
            sys_test("Hello from sleep_test, tick = 1\n\r");
        }

        tick = !tick;
        
        sleep(1000);        
    }

    return 0;
}