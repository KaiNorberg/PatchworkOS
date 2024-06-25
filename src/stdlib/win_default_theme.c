#include <sys/win.h>

void win_default_theme(win_theme_t* theme)
{
    theme->edgeWidth = 3;
    theme->ridgeWidth = 2;
    theme->highlight = 0xFFFCFCFC;
    theme->shadow = 0xFF232629;
    theme->background = 0xFFBFBFBF;
    theme->selected = 0xFF00007F;
    theme->unSelected = 0xFF7F7F7F;
    theme->topbarHeight = 32;
}
