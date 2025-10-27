#include "builtin.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/io.h>
#include <sys/proc.h>

static uint64_t builtin_cd(uint64_t argc, const char** argv);
static uint64_t builtin_help(uint64_t argc, const char** argv);

static builtin_t builtins[] = {
    {
    .name = "cd",
    .callback = builtin_cd,
    .description = "Change the current working directory.",
    .usage = "cd [directory]"
},
{
    .name = "help",
    .callback = builtin_help,
    .description = "Show this help message.",
    .usage = "help"
},};

static uint64_t builtin_cd(uint64_t argc, const char** argv)
{
    if (argc < 2)
    {
        chdir("/usr");
        return 0;
    }

    if (chdir(argv[1]) == ERR)
    {
        fprintf(stderr, "cd: %s\n", strerror(errno));
        return ERR;
    }

    return 0;
}

static uint64_t builtin_help(uint64_t argc, const char** argv)
{
    (void)argc;
    (void)argv;

    printf("\033[33mBUILTINS:\033[0m\n");
    for (uint64_t i = 0; i < sizeof(builtins) / sizeof(builtins[0]); i++)
    {
        printf("  %-20s\033[0m \t\033[90m%s\033[0m\n", builtins[i].usage, builtins[i].description);
    }

    printf("\n\033[33mFEATURES:\033[0m\n");
    printf("  command1 | command2\t\033[90mPipe output\033[0m\n");
    printf("  command > file     \t\033[90mRedirect standard output\033[0m\n");
    printf("  command < file     \t\033[90mRedirect standard input\033[0m\n");
    printf("\n\033[33mKEYBINDINGS:\033[0m\n");
    printf("  Enter/Left/Right   \t\033[90mEdit input\033[0m\n");
    printf("  Up/Down            \t\033[90mNavigate history\033[0m\n");
    printf("  Ctrl+C             \t\033[90mTerminate process\033[0m\n");

    printf("\n\033[90mExternal commands are executed from /bin and /usr/bin.\033[0m\n");

    return 0;
}

bool builtin_exists(const char* name)
{
    for (uint64_t i = 0; i < sizeof(builtins) / sizeof(builtins[0]); i++)
    {
        if (strcmp(name, builtins[i].name) == 0)
        {
            return true;
        }
    }

    return false;
}

uint64_t builtin_execute(uint64_t argc, const char** argv)
{
    if (argc == 0)
    {
        return 0;
    }

    for (uint64_t i = 0; i < sizeof(builtins) / sizeof(builtins[0]); i++)
    {
        if (strcmp(argv[0], builtins[i].name) == 0)
        {
            builtins[i].callback(argc, argv);
            return 0;
        }
    }

    return ERR;
}
