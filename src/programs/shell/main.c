#include "cursor.h"
#include "shell.h"
#include "taskbar.h"
#include "wall.h"

int main(void)
{
    shell_init();

    shell_push(wall_new());
    shell_push(taskbar_new());
    shell_push(cursor_new());

    shell_loop();

    return 0;
}
