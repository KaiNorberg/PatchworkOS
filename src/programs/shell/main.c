#include "fb.h"
#include "terminal.h"
#include "token.h"
#include "parser.h"
#include "input.h"

#include <stdlib.h>
#include <sys/io.h>
#include <sys/proc.h>
#include <sys/win.h>

const char* command_read(void)
{
    static char command[TERMINAL_MAX_COMMAND];
    command[0] = '\0';

    char cwd[256];
    realpath(cwd, ".");

    terminal_print("\n");
    terminal_print(cwd);
    terminal_print("\n\033[35m>\033[m ");

    uint64_t length = 0;
    while (1)
    {
        terminal_update_cursor();

        char chr = input_kbd_read(TERMINAL_BLINK_INTERVAL / 5);

        if (chr == '\b')
        {
            if (length != 0)
            {
                command[length - 1] = '0';

                terminal_put('\b');
                length--;
            }
        }
        else if (chr == '\n')
        {
            terminal_put('\n');
            return command;
        }
        else if (chr != '\0' && length < TERMINAL_MAX_COMMAND)
        {
            command[length] = chr;
            command[length + 1] = '\0';

            terminal_put(chr);
            length++;
        }
    }
}

int main(void)
{    
    fb_init();
    terminal_init();
    input_init();

    /*fd_t window = open("sys:/srv/win");
    if (window == ERR)
    {
        terminal_error("open");
        return EXIT_FAILURE;
    }

    win_info_t initInfo;
    initInfo.width = 500;
    initInfo.height = 400;
    initInfo.x = WIN_DEFAULT;
    initInfo.y = WIN_DEFAULT;
    if (write(window, &initInfo, sizeof(win_info_t)))
    {
        terminal_error("write");
        return EXIT_FAILURE;
    }

    close(window);*/

    while (1)
    {
        parser_parse(command_read());
    }

    return EXIT_SUCCESS;
}