#include <sys/win.h>

void win_default_theme(win_theme_t* theme)
{
    theme->edgeWidth = 4;
    theme->highlight = 0xFFFEFEFE;
    theme->shadow = 0xFF3C3C3C;
    theme->background = 0xFFBFBFBF;
    theme->topbarHighlight = 0xFF00007F;
    theme->topbarShadow = 0xFF7F7F7F;
    theme->topbarHeight = 32;
}
