#include "fb.h"
#include "terminal.h"

#include <stdlib.h>

int main(void)
{
    fb_init();
    terminal_init();

    while (1)
    {
        const char* command = terminal_read();
        terminal_print(command);
    }

    return EXIT_SUCCESS;
}