#include "command.h"

#include <stdbool.h>
#include <stdio.h>
#include <sys/io.h>
#include <sys/proc.h>

void print_prompt(void)
{
    char cwd[MAX_PATH];
    fd_t fd = pid_open(process_id(), "cwd");
    read(fd, cwd, MAX_PATH);
    close(fd);

    printf("\n%s\n> ", cwd);
}

bool read_command(char* buffer, uint64_t size)
{
    uint64_t index = 0;
    buffer[0] = '\0';
    while (1)
    {
        char chr;
        if (read(STDIN_FILENO, &chr, 1) == 0)
        {
            return false;
        }
        else if (chr == '\n')
        {
            buffer[index] = '\0';
            return true;
        }

        if (index < MAX_PATH)
        {
            buffer[index++] = chr;
        }
    }
}

int main(void)
{
    printf("Welcome to the Shell (Very WIP)\n");
    printf("Type help for a list of commands\n");

    while (1)
    {
        print_prompt();

        char buffer[MAX_PATH];
        if (!read_command(buffer, MAX_PATH))
        {
            break;
        }

        command_execute(buffer);
    }
    return 0;
}
