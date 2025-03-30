#include "command.h"
#include "terminal.h"

#include <sys/io.h>
#include <sys/proc.h>

void print_prompt(void)
{
    char cwd[MAX_PATH];
    realpath(cwd, ".");

    terminal_print("\n%s\n> ", cwd);
}

void read_command(char* buffer, uint64_t size)
{
    uint64_t index = 0;
    buffer[0] = '\0';
    while (1)
    {
        char chr = terminal_input();
        terminal_print("%c", chr);
        switch (chr)
        {
        case '\n':
        {
            return;
        }
        break;
        case '\b':
        {
            if (index != 0)
            {
                buffer[--index] = '\0';
            }
        }
        break;
        default:
        {
            if (index != size)
            {
                buffer[index++] = chr;
                buffer[index] = '\0';
            }
        }
        break;
        }
    }
}

int main(void)
{
    terminal_init();

    terminal_print("Welcome to the Terminal (Very WIP)\n");
    terminal_print("Type help for a list of commands\n");

    while (1)
    {
        print_prompt();

        char buffer[MAX_PATH];
        read_command(buffer, MAX_PATH);

        command_execute(buffer);
    }

    terminal_deinit();
    return 0;
}
