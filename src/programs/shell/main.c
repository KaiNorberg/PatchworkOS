#include "pipeline.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/io.h>
#include <sys/proc.h>

void prompt_print(void)
{
    char cwd[MAX_PATH];
    fd_t fd = open("/proc/self/cwd");
    if (fd == ERR)
    {
        printf("\n?> ");
        fflush(stdout);
        return;
    }

    uint64_t length = read(fd, cwd, MAX_PATH);
    cwd[length] = '\0';
    close(fd);
    printf("\n%s\n> ", cwd);
    fflush(stdout);
}

static uint64_t cmdline_read(char* buffer, uint64_t size)
{
    uint64_t index = 0;
    buffer[0] = '\0';
    while (1)
    {
        char chr;
        if (read(STDIN_FILENO, &chr, 1) == 0)
        {
            return ERR;
        }
        else if (chr == '\n')
        {
            buffer[index] = '\0';
            return 0;
        }
        if (index + 1 < size)
        {
            buffer[index++] = chr;
        }
    }
}

void join_args(char* buffer, uint64_t size, int argc, char* argv[])
{
    buffer[0] = '\0';
    uint64_t pos = 0;

    for (int i = 1; i < argc && pos < size - 1; i++)
    {
        if (i > 1 && pos < size - 1)
        {
            buffer[pos++] = ' ';
        }

        char* arg = argv[i];
        while (*arg && pos < size - 1)
        {
            buffer[pos++] = *arg++;
        }
    }

    buffer[pos] = '\0';
}

int execute_command(const char* cmdline)
{
    pipeline_t pipeline;
    if (pipeline_init(&pipeline, cmdline) == ERR)
    {
        return EXIT_FAILURE;
    }
    return pipeline_execute(&pipeline) == ERR ? EXIT_FAILURE : EXIT_SUCCESS;
}

void run_interactive_shell(void)
{
    printf("Welcome to the Shell (Very WIP)\n");
    printf("Type help for a list of commands\n");

    while (1)
    {
        prompt_print();
        char cmdline[MAX_PATH];
        if (cmdline_read(cmdline, MAX_PATH) == ERR)
        {
            break;
        }
        execute_command(cmdline);
    }
}

int main(int argc, char* argv[])
{
    if (argc > 1)
    {
        char cmdline[MAX_PATH];
        join_args(cmdline, MAX_PATH, argc, argv);
        return execute_command(cmdline);
    }
    else
    {
        run_interactive_shell();
        return 0;
    }
}
