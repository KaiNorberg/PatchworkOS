#include "command.h"

#include "terminal.h"
#include "token.h"

#include <stdlib.h>
#include <string.h>
#include <sys/io.h>
#include <sys/proc.h>

// TODO: These commands should probobly be script files or something.

static void command_cd(const char* token)
{
    if (token == NULL)
    {
        chdir("home:/usr");
        return;
    }

    if (chdir(token) == ERR)
    {
        terminal_error(NULL);
    }
}

static void command_ls(const char* token)
{
    char path[MAX_PATH];
    if (token == NULL)
    {
        strcpy(path, ".");
    }
    else
    {
        token_copy(path, token);
    }

    dir_entry_t* entries;
    uint64_t amount = loaddir(&entries, path);

    for (uint64_t i = 0; i < amount; i++)
    {
        terminal_print(entries[i].name);
        if (entries[i].type == STAT_DIR)
        {
            terminal_put('/');
        }
        terminal_put(' ');
    }

    terminal_put('\n');
    free(entries);
}

static void command_clear(const char* token)
{
    terminal_clear();
}

static void command_help(const char* token);

static command_t commands[] = {
    {
        .name = "cd",
        .synopsis = "cd [DIRECTORY]",
        .description =
            "If DIRECTORY is given, the current working directory will be set to DIRECTORY else it will be set to \"home:/usr\"",
        .callback = command_cd,
    },
    {
        .name = "ls",
        .synopsis = "ls [DIRECTORY]",
        .description =
            "If DIRECTORY is given, the contents of DIRECTORY will be printed, else the contents of the working "
            "directory will be printed. Any printed entry followed by a \"/\" is a directory, other entries are files.",
        .callback = command_ls,
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

static void command_help(const char* token)
{
    if (token == NULL)
    {
        terminal_print("Type help [COMMAND] for more information about COMMAND\n  ");
        for (uint64_t i = 0; i < sizeof(commands) / sizeof(commands[0]); i++)
        {
            terminal_print(commands[i].name);
            terminal_print(" ");
        }
        terminal_print("./[BINARY IN CWD] [BINARY IN LOOKUP PATHS]");
    }
    else
    {
        command_t* command = NULL;
        for (uint64_t i = 0; i < sizeof(commands) / sizeof(commands[0]); i++)
        {
            if (token_equal(token, commands[i].name))
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

bool command_look_for_binary(const char* binary, const char* path)
{
    dir_entry_t* entries;
    uint64_t amount = loaddir(&entries, path);

    for (uint64_t i = 0; i < amount; i++)
    {
        if (strcmp(binary, entries[i].name) == 0)
        {
            free(entries);
            return true;
        }
    }

    free(entries);
    return false;
}

// TODO: Config file?
static const char* lookupDirs[] = {
    "home:/bin",
    "home:/usr/bin",
};

void command_spawn(const char* dir, const char* command)
{
    char binary[MAX_PATH];
    token_copy(binary, command);

    for (uint64_t i = 0; i < sizeof(lookupDirs) / sizeof(lookupDirs[0]); i++)
    {
        if (command_look_for_binary(binary, lookupDirs[i]))
        {
            char path[MAX_PATH] = {0};
            strcpy(path, lookupDirs[i]);
            strcat(path, "/");
            strcat(path, binary);
            spawn(path);
            return;
        }
    }
}

void command_parse(const char* command)
{
    if (command[0] == '.' && command[1] == '/')
    {
        command_spawn(".", command + 2);
        return;
    }

    for (uint64_t i = 0; i < sizeof(commands) / sizeof(commands[0]); i++)
    {
        if (token_equal(command, commands[i].name))
        {
            commands[i].callback(token_next(command));
            return;
        }
    }

    char binary[MAX_PATH];
    token_copy(binary, command);
    for (uint64_t i = 0; i < sizeof(lookupDirs) / sizeof(lookupDirs[0]); i++)
    {
        if (command_look_for_binary(binary, lookupDirs[i]))
        {
            command_spawn(lookupDirs[i], command);
            return;
        }
    }

    terminal_error("command not found");
}
