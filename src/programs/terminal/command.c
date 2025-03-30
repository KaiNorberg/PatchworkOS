#include "command.h"

#include "terminal.h"
#include "token.h"

#include <stdlib.h>
#include <string.h>
#include <sys/argsplit.h>
#include <sys/io.h>
#include <sys/proc.h>

// TODO: These commands should probobly be script files or something.

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
        terminal_error(NULL);
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
        terminal_print("Type help [COMMAND] for more information about COMMAND\n  ");
        for (uint64_t i = 0; i < sizeof(commands) / sizeof(commands[0]); i++)
        {
            terminal_print("%s ", commands[i].name);
        }
        terminal_print("./[BINARY IN CWD] [BINARY IN home:/bin OR home:/bin/usr/]");
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
            terminal_error("command not found");
            return;
        }

        terminal_print("NAME\n  ");
        terminal_print(command->name);

        terminal_print("\nSYNOPSIS\n  ");
        terminal_print(command->synopsis);

        terminal_print("\nDESCRIPTION\n  ");
        terminal_print(command->description);
    }
}

static uint64_t command_spawn(const char** argv)
{
    pipefd_t childStdin;
    pipefd_t childStdout;

    pipe(&childStdin);
    pipe(&childStdout);

    spawn_fd_t fds[] = {{.child = STDIN_FILENO, .parent = childStdin.read}, {.child = STDOUT_FILENO, .parent = childStdout.write}, SPAWN_FD_END};
    pid_t pid = spawn(argv, fds);
    if (pid == ERR)
    {
        close(childStdin.write);
        close(childStdin.read);
        close(childStdout.write);
        close(childStdout.read);
        return ERR;
    }

    close(childStdin.read);
    close(childStdout.write);
    
    while (1)
    {
        char chr;
        if (read(childStdout.read, &chr, 1) == 0)
        {
            break;
        }

        terminal_print("%c", chr);
    }

    close(childStdin.write);
    close(childStdout.read);
    return 0;
}

void command_execute(const char* command)
{
    uint64_t argc;
    const char** argv = argsplit(command, &argc);
    if (argc == 0 || argv == NULL)
    {
        terminal_print("empty command");
        return;
    }

    if (argv[0][0] == '.' && argv[0][1] == '/')
    {
        stat_t info;
        if (stat(argv[0], &info) != ERR && info.type == STAT_FILE)
        {
            if (command_spawn(argv) == ERR)
            {
                terminal_error(NULL);
            }
            return;
        }
    }

    for (uint64_t i = 0; i < sizeof(commands) / sizeof(commands[0]); i++)
    {
        if (token_equal(command, commands[i].name))
        {
            commands[i].callback(argc, argv);
            return;
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
                terminal_error(NULL);
            }
            return;
        }
    }

    terminal_error("command not found");
}
