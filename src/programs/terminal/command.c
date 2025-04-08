#include "command.h"

#include "terminal.h"
#include "token.h"

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

static uint64_t spawn_with_redirection(const char** argv, fd_t in_fd, fd_t out_fd)
{
    fd_t childStdin[2];
    if (in_fd == STDIN_FILENO)
    {
        if (open2("sys:/pipe/new", childStdin) == ERR)
            return ERR;
    }
    else
    {
        childStdin[PIPE_READ] = in_fd;
        childStdin[PIPE_WRITE] = -1; // Mark as not ours to close
    }

    fd_t childStdout[2];
    if (out_fd == STDOUT_FILENO)
    {
        if (open2("sys:/pipe/new", childStdout) == ERR)
        {
            if (in_fd == STDIN_FILENO)
                close(childStdin[PIPE_READ]);
            return ERR;
        }
    }
    else
    {
        childStdout[PIPE_WRITE] = out_fd;
        childStdout[PIPE_READ] = -1; // Mark as not ours to close
    }

    spawn_fd_t fds[] = {{.child = STDIN_FILENO, .parent = childStdin[PIPE_READ]},
        {.child = STDOUT_FILENO, .parent = childStdout[PIPE_WRITE]}, SPAWN_FD_END};

    pid_t pid = spawn(argv, fds);
    if (pid == ERR)
    {
        if (in_fd == STDIN_FILENO)
        {
            close(childStdin[PIPE_WRITE]);
            close(childStdin[PIPE_READ]);
        }
        if (out_fd == STDOUT_FILENO)
        {
            close(childStdout[PIPE_WRITE]);
            close(childStdout[PIPE_READ]);
        }
        return ERR;
    }

    // Close ends we don't need
    if (in_fd == STDIN_FILENO)
        close(childStdin[PIPE_READ]);
    if (out_fd == STDOUT_FILENO)
        close(childStdout[PIPE_WRITE]);

    // Read output if not redirected
    if (out_fd == STDOUT_FILENO)
    {
        while (1)
        {
            char chr;
            if (read(childStdout[PIPE_READ], &chr, 1) == 0)
                break;
            terminal_print("%c", chr);
        }
        close(childStdout[PIPE_READ]);
    }

    if (in_fd == STDIN_FILENO)
        close(childStdin[PIPE_WRITE]);
    return 0;
}

static uint64_t handle_redirection(const char* filename)
{
    fd_t fd = open(filename);
    if (fd == ERR)
    {
        terminal_error("failed to open file");
        return ERR;
    }
    return fd;
}

static uint64_t command_spawn(const char** argv)
{
    fd_t childStdin[2];
    if (open2("sys:/pipe/new", childStdin) == ERR)
    {
        return ERR;
    }
    fd_t childStdout[2];
    if (open2("sys:/pipe/new", childStdout) == ERR)
    {
        return ERR;
    }

    spawn_fd_t fds[] = {{.child = STDIN_FILENO, .parent = childStdin[PIPE_READ]},
        {.child = STDOUT_FILENO, .parent = childStdout[PIPE_WRITE]}, SPAWN_FD_END};
    pid_t pid = spawn(argv, fds);
    if (pid == ERR)
    {
        close(childStdin[PIPE_WRITE]);
        close(childStdin[PIPE_READ]);
        close(childStdout[PIPE_WRITE]);
        close(childStdout[PIPE_READ]);
        return ERR;
    }

    close(childStdin[PIPE_READ]);
    close(childStdout[PIPE_WRITE]);

    while (1)
    {
        char chr;
        if (read(childStdout[PIPE_READ], &chr, 1) == 0)
        {
            break;
        }

        terminal_print("%c", chr);
    }

    close(childStdin[PIPE_WRITE]);
    close(childStdout[PIPE_READ]);
    return 0;
}

void command_execute(const char* command)
{
    // Split into commands separated by pipes
    const char* pipe_pos = strchr(command, '|');
    if (pipe_pos || strstr(command, ">"))
    {
        // Handle pipes and redirections
        uint64_t argc;
        const char** argv = argsplit(command, &argc);

        fd_t in_fd = STDIN_FILENO;
        fd_t out_fd = STDOUT_FILENO;
        const char* output_file = NULL;

        // Parse redirections
        for (uint64_t i = 0; i < argc; i++)
        {
            if (strcmp(argv[i], ">") == 0 && i + 1 < argc)
            {
                output_file = argv[i + 1];
                out_fd = handle_redirection(output_file);
                if (out_fd == ERR)
                    return;
                argc = i; // Truncate command at redirection
                break;
            }
        }

        // Handle simple commands with redirection
        if (!pipe_pos && output_file)
        {
            if (argv[0][0] == '.' && argv[0][1] == '/')
            {
                stat_t info;
                if (stat(argv[0], &info) != ERR && info.type == STAT_FILE)
                {
                    if (spawn_with_redirection(argv, in_fd, out_fd) == ERR)
                    {
                        terminal_error(NULL);
                    }
                    close(out_fd);
                    return;
                }
            }

            // [Rest of command handling remains similar...]
        }

        // Handle pipes (more complex case)
        if (pipe_pos)
        {
            // [Full pipe implementation would go here...]
            terminal_error("pipes not fully implemented yet");
        }

        if (out_fd != STDOUT_FILENO)
            close(out_fd);
        return;
    }

    // Original non-pipe/redirection handling
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