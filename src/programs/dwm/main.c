#include "dwm.h"

int main(void)
{
    dwm_init();

    dwm_loop();

    dwm_deinit();
}
