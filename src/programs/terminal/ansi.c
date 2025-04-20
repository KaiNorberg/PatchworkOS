#include "ansi.h"

#include <sys/win.h>

void ansi_init(ansi_t* ansi)
{
    ansi->foreground = winTheme.bright;
    ansi->background = winTheme.dark;
    ansi->bold = false;
    ansi->italic = false;
    ansi->underline = false;
    ansi->inverse = false;
    ansi->escaped = false;
    ansi->csi = false;
}
