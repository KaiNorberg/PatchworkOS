#include "ansi.h"

#include <libdwm/dwm.h>

void ansi_init(ansi_t* ansi)
{
    ansi->foreground = windowTheme.bright;
    ansi->background = windowTheme.dark;
    ansi->bold = false;
    ansi->italic = false;
    ansi->underline = false;
    ansi->inverse = false;
    ansi->escaped = false;
    ansi->csi = false;
}
