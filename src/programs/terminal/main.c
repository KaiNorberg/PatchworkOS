#include "terminal.h"

int main(void)
{
    terminal_init();

    terminal_loop();

    terminal_cleanup();
    return 0;
}
