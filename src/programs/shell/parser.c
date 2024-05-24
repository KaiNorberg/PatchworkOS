#include "parser.h"

#include "terminal.h"

#include <errno.h>
#include <sys/io.h>

static void command_cd(Token* token)
{
    if (!token_next(token))
    {
        terminal_error("invalid argument");
        return;
    }

    char path[TERMINAL_MAX_COMMAND];
    if (token_string(token, path, TERMINAL_MAX_COMMAND) == ERR)
    {
        terminal_error(strerror(errno));
        return;
    }

    if (chdir(path) == ERR)
    {
        terminal_error(strerror(errno));
        return;
    }
}

static Command commands[] = 
{
    {"cd", command_cd}
};

void parser_parse(const char* string)
{
    Token token = token_first(string);

    for (uint64_t i = 0; i < sizeof(commands) / sizeof(commands[0]); i++)
    {
        if (token_compare(&token, commands[i].name))
        {
            commands[i].callback(&token);
            return;
        }
    }

    terminal_error("command not found");
}