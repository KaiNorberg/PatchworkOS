#include "pipeline.h"

#include <stdbool.h>
#include <stdio.h>
#include <sys/io.h>
#include <sys/proc.h>

void prompt_print(void)
{
    char cwd[MAX_PATH];
    fd_t fd = open("sys:/proc/self/cwd");
    read(fd, cwd, MAX_PATH);
    close(fd);

    printf("\n%s\n> ", cwd);
}

bool cmdline_read(char* buffer, uint64_t size)
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

        if (index + 1 < size)
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
        prompt_print();

        char cmdline[MAX_PATH];
        if (!cmdline_read(cmdline, MAX_PATH - 2))
        {
            break;
        }

        pipeline_t pipeline;
        if (pipeline_init(&pipeline, cmdline) == ERR)
        {
            printf("error: unable to parse pipeline");
            continue;
        }

        pipeline_execute(&pipeline);
    }
    return 0;
}
