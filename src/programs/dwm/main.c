#include "dwm.h"
#include "screen.h"
#include "compositor.h"

int main(void)
{
    dwm_init();
    screen_init();
    compositor_init();

    dwm_loop();

    screen_deinit();
    dwm_deinit();
    return 0;
}
