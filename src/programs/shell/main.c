#include "fb.h"
#include "terminal.h"

int main(void)
{
    fb_init();
    terminal_init();

    terminal_loop();
}