#include "command.h"

#include "terminal.h"
#include "token.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/argsplit.h>
#include <sys/io.h>
#include <sys/proc.h>

// TODO: These commands should probobly be script files or something. Lua? Custom?

static const char* lookupDirs[] = {
    "home:/bin",
    "home:/usr/bin",
};

static void command_cd(uint64_t argc, const char** argv)
{
    if (argc < 2)
    {
        chdir("home:/usr");
        return;
    }

    if (chdir(argv[1]) == ERR)
    {
        printf("error: %s", strerror(errno));
    }
}

static void command_clear(uint64_t argc, const char** argv)
{
    terminal_clear();
}

static void command_help(uint64_t argc, const char** argv);

static command_t commands[] = {
    {
        .name = "cd",
        .synopsis = "cd [DIRECTORY]",
        .description =
            "If DIRECTORY is given, the current working directory will be set to DIRECTORY else it will be set to \"home:/usr\"",
        .callback = command_cd,
    },
    {
        .name = "clear",
        .synopsis = "clear",
        .description = "Clears the screen",
        .callback = command_clear,
    },
    {
        .name = "help",
        .synopsis = "help [COMMAND]",
        .description =
            "If COMMAND is given, information about COMMAND will be printed, else a list of available commands will be printed.",
        .callback = command_help,
    },
};

static void command_help(uint64_t argc, const char** argv)
{
    if (argc < 2)
    {
        printf("Type help [COMMAND] for more information about COMMAND\n  ");
        for (uint64_t i = 0; i < sizeof(commands) / sizeof(commands[0]); i++)
        {
            printf("%s ", commands[i].name);
        }
        printf("./[BINARY IN CWD] [BINARY IN home:/bin OR home:/bin/usr/]");
    }
    else
    {
        command_t* command = NULL;
        for (uint64_t i = 0; i < sizeof(commands) / sizeof(commands[0]); i++)
        {
            if (token_equal(argv[1], commands[i].name))
            {
                command = &commands[i];
                break;
            }
        }
        if (command == NULL)
        {
            printf("error: command not found");
            return;
        }

        printf("NAME\n  ");
        printf(command->name);

        printf("\nSYNOPSIS\n  ");
        printf(command->synopsis);

        printf("\nDESCRIPTION\n  ");
        printf(command->description);
    }
}

static uint64_t command_spawn(const char** argv)
{
    spawn_fd_t fds[] = {
        {.child = STDIN_FILENO, .parent = STDIN_FILENO},
        {.child = STDOUT_FILENO, .parent = STDOUT_FILENO},
        SPAWN_FD_END,
    };
    pid_t pid = spawn(argv, fds);
    if (pid == ERR)
    {
        return ERR;
    }

    fd_t child = procfd(pid);
    writef(child, "wait");
    close(child);

    return 0;
}

void command_execute(const char* command)
{
    uint64_t argc;
    const char** argv = argsplit(command, &argc);
    if (argc == 0 || argv == NULL)
    {
        goto end_of_func;
    }

    for (int64_t i = 0; i < (int64_t)argc; i++)
    {
        if (strcmp(argv[i], ">") == 0)
        {
            if (i != (int64_t)argc - 2)
            {
                printf("error: invalid command format");
                goto end_of_func;
            }
            
            const char* target = argv[argc - 1];
            fd_t fd = openas(STDOUT_FILENO, target);
        }
    }

    if (argv[0][0] == '.' && argv[0][1] == '/')
    {
        stat_t info;
        if (stat(argv[0], &info) != ERR && info.type == STAT_FILE)
        {
            if (command_spawn(argv) == ERR)
            {
                printf("error: %s", strerror(errno));
            }
            goto end_of_func;
        }
    }

    for (uint64_t i = 0; i < sizeof(commands) / sizeof(commands[0]); i++)
    {
        if (token_equal(command, commands[i].name))
        {
            commands[i].callback(argc, argv);
            goto end_of_func;
        }
    }

    for (uint64_t i = 0; i < sizeof(lookupDirs) / sizeof(lookupDirs[0]); i++)
    {
        if (strlen(lookupDirs[i]) + strlen(argv[0]) + 1 >= MAX_PATH)
        {
            continue;
        }

        char path[MAX_PATH];
        strcpy(path, lookupDirs[i]);
        strcat(path, "/");
        strcat(path, argv[0]);

        stat_t info;
        if (stat(path, &info) != ERR && info.type == STAT_FILE)
        {
            argv[0] = path;
            if (command_spawn(argv) == ERR)
            {
                printf("error: %s", strerror(errno));
            }
            goto end_of_func;
        }
    }

    printf("error: command not found");
end_of_func:
    terminal_reset_stdio();
}