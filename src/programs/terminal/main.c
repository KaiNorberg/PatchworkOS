#include <stdlib.h>

#include "terminal.h"

int main(void)
{
    display_t* disp = display_new();
    if (disp == NULL)
    {
        return EXIT_FAILURE;
    }

    window_t* term = terminal_new(disp);
    if (term == NULL)
    {
        display_free(disp);
        return EXIT_FAILURE;
    }

    event_t event = {0};
    while (display_next(disp, &event, CLOCKS_NEVER) != ERR)
    {
        display_dispatch(disp, &event);
    }

    window_free(term);
    display_free(disp);
    return 0;
}
