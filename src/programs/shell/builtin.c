#include "builtin.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/io.h>
#include <sys/proc.h>

static void builtin_cd(uint64_t argc, const char** argv)
{
    if (argc < 2)
    {
        chdir("/usr");
        return;
    }

    if (chdir(argv[1]) == ERR)
    {
        fprintf(stderr, "cd: %s\n", strerror(errno));
    }
}

static void builtin_clear(uint64_t argc, const char** argv)
{
}

static void builtin_help(uint64_t argc, const char** argv);

static builtin_t builtins[] = {
    {
        .name = "cd",
        .synopsis = "cd [DIRECTORY]",
        .description = "If DIRECTORY is given, the current working directory will be set to DIRECTORY else it will be "
                       "set to \"/usr\"",
        .callback = builtin_cd,
    },
    /*{
        .name = "clear",
        .synopsis = "clear",
        .description = "Clears the screen",
        .callback = builtin_clear,
    },*/
    {
        .name = "help",
        .synopsis = "help [builtin]",
        .description = "If builtin is given, information about builtin will be printed, else a list of available "
                       "builtins will be printed.",
        .callback = builtin_help,
    },
};

static void builtin_help(uint64_t argc, const char** argv)
{
    if (argc < 2)
    {
        printf("Type help [builtin] for more information about builtin\n  ");
        for (uint64_t i = 0; i < sizeof(builtins) / sizeof(builtins[0]); i++)
        {
            printf("%s ", builtins[i].name);
        }
        printf("./[BINARY IN CWD] [BINARY IN /bin OR /bin/usr/]");
    }
    else
    {
        builtin_t* builtin = NULL;
        for (uint64_t i = 0; i < sizeof(builtins) / sizeof(builtins[0]); i++)
        {
            if (strcmp(argv[1], builtins[i].name) == 0)
            {
                builtin = &builtins[i];
                break;
            }
        }
        if (builtin == NULL)
        {
            printf("error: builtin not found");
            return;
        }

        printf("NAME\n  ");
        printf(builtin->name);

        printf("\nSYNOPSIS\n  ");
        printf(builtin->synopsis);

        printf("\nDESCRIPTION\n  ");
        printf(builtin->description);
    }
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

void builtin_execute(uint64_t argc, const char** argv)
{
    if (argc == 0)
    {
        return;
    }

    for (uint64_t i = 0; i < sizeof(builtins) / sizeof(builtins[0]); i++)
    {
        if (strcmp(argv[0], builtins[i].name) == 0)
        {
            builtins[i].callback(argc, argv);
            return;
        }
    }
}
