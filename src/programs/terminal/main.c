#include "terminal.h"

#include <sys/proc.h>

int main(void)
{
    terminal_init();

    terminal_loop();

    terminal_deinit();
    return 0;
}
