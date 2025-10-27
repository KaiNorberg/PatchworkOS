#include "builtin.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/io.h>
#include <sys/proc.h>

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
    (void)argc; // Unused
    (void)argv; // Unused

    printf("BUILT-IN COMMANDS: ");
    builtin_dump_list();
    printf("\n\n");

    printf("USAGE:\n");
    printf("  Navigate:  cd [dir], ls [dir]\n");
    printf("  Redirect:  command > file, command < file\n");
    printf("  Pipe:      command1 | command2\n");
    printf("  Interrupt: ctrl+C (terminate process)\n");
    printf("  History:   up/down arrows to navigate command history\n");
    printf("\n");
    printf("Commands can be found in /bin and /usr/bin.\n");

    return 0;
}

static builtin_t builtins[] = {{
    .name = "cd",
    .callback = builtin_cd,
},
{
    .name = "help",
    .callback = builtin_help,
},};

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

void builtin_dump_list(void)
{
    for (uint64_t i = 0; i < sizeof(builtins) / sizeof(builtins[0]); i++)
    {
        printf("%s", builtins[i].name);
        if (i + 1 < sizeof(builtins) / sizeof(builtins[0]))
        {
            printf(", ");
        }
    }
}
