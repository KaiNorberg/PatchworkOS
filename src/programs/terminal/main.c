#include "command.h"
#include "terminal.h"

#include <stdio.h>
#include <sys/io.h>
#include <sys/proc.h>

void print_prompt(void)
{
    char cwd[MAX_PATH];
    realpath(cwd, ".");

    printf("\n%s\n> ", cwd);
}

void read_command(char* buffer, uint64_t size)
{
    uint64_t index = 0;
    buffer[0] = '\0';
    while (1)
    {
        char chr;
        read(STDIN_FILENO, &chr, 1);

        if (terminal_should_quit())
        {
            break;
        }

        switch (chr)
        {
        case '\n':
        {
            printf("%c", chr);
            return;
        }
        break;
        case '\b':
        {
            if (index != 0)
            {
                printf("%c", chr);
                buffer[--index] = '\0';
            }
        }
        break;
        default:
        {
            if (index != size)
            {
                printf("%c", chr);
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

    printf("Welcome to the Terminal (Very WIP)\n");
    printf("Type help for a list of commands\n");

    while (1)
    {
        print_prompt();

        char buffer[MAX_PATH];
        read_command(buffer, MAX_PATH);

        if (terminal_should_quit())
        {
            break;
        }

        command_execute(buffer);
    }

    terminal_deinit();
    return 0;
}
