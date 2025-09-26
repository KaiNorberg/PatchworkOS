#include "ansi.h"

#include <string.h>
#include <libpatchwork/patchwork.h>

void ansi_init(ansi_t* ansi)
{
    memset(ansi, 0, sizeof(ansi_t));
    /*ansi->foreground = 0;
    ansi->background = 0;
    ansi->bold = false;
    ansi->italic = false;
    ansi->underline = false;
    ansi->inverse = false;
    ansi->escaped = false;
    ansi->csi = false;*/
}
