#include "terminal.h"

int main(void)
{
    terminal_t term;
    terminal_init(&term);

    while (terminal_update(&term))
        ;

    terminal_deinit(&term);
    return 0;
}
