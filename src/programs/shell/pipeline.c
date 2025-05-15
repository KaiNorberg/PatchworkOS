#include "pipeline.h"
#include "builtin.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/argsplit.h>
#include <sys/io.h>
#include <sys/proc.h>

static const char* lookupDirs[] = {
    "home:/bin",
    "home:/usr/bin",
};

uint64_t pipeline_init(pipeline_t* pipeline, const char* cmdline)
{
    uint64_t tokenAmount;
    const char** tokens = argsplit(cmdline, UINT64_MAX, &tokenAmount);
    if (tokens == NULL)
    {
        return ERR;
    }

    if (tokenAmount == 0)
    {
        pipeline->cmds = NULL;
        pipeline->capacity = tokenAmount;
        pipeline->amount = 0;
        free(tokens);
        return 0;
    }

    pipeline->cmds = malloc(sizeof(cmd_t) * tokenAmount);
    pipeline->capacity = tokenAmount;
    pipeline->amount = 0;

    for (uint64_t i = 0; i < tokenAmount; i++)
    {
        cmd_t* cmd = &pipeline->cmds[i];
        cmd->argv = NULL;
        cmd->argc = 0;
        cmd->stdin = STDIN_FILENO;
        cmd->stdout = STDOUT_FILENO;
        cmd->stderr = STDERR_FILENO;
        cmd->closeStdin = false;
        cmd->closeStdout = false;
        cmd->closeStderr = false;
    }

    uint64_t currentCmd = 0;
    uint64_t currentArg = 0;
    const char** currentArgv = malloc(sizeof(char*) * (tokenAmount + 1));
    for (uint64_t i = 0; i < tokenAmount; i++)
    {
        if (strcmp(tokens[i], "|") == 0)
        {
            if (currentArg == 0)
            {
                printf("error: empty command in pipeline\n");
                goto token_parse_error;
            }

            currentArgv[currentArg] = NULL;

            cmd_t* cmd = &pipeline->cmds[currentCmd];
            cmd->argv = currentArgv;
            cmd->argc = currentArg;

            fd_t pipe[2];
            if (open2("sys:/pipe/new", pipe) == ERR)
            {
                printf("error: unable to open pipe (%s)\n", strerror(errno));
                goto token_parse_error;
            }

            cmd->stdout = pipe[PIPE_WRITE];
            cmd->closeStdout = true;

            currentCmd++;
            currentArg = 0;
            currentArgv = malloc(sizeof(char*) * (tokenAmount + 1));

            cmd_t* nextCmd = &pipeline->cmds[currentCmd];
            nextCmd->stdin = pipe[PIPE_READ];
            nextCmd->closeStdin = true;
        }
        else if (strcmp(tokens[i], "<") == 0)
        {
            if (i + 1 >= tokenAmount)
            {
                printf("error: missing filename after <\n");
                goto token_parse_error;
            }

            fd_t fd = open(tokens[i + 1]);
            if (fd == ERR)
            {
                printf("error: unable to open %s (%s)\n", tokens[i + 1], strerror(errno));
                goto token_parse_error;
            }

            cmd_t* cmd = &pipeline->cmds[currentCmd];
            if (cmd->closeStdin)
            {
                close(cmd->stdin);
            }
            cmd->stdin = fd;
            cmd->closeStdin = true;

            i++; // Skip file
        }
        else if (strcmp(tokens[i], ">") == 0)
        {
            if (i + 1 >= tokenAmount)
            {
                printf("error: missing filename after >\n");
                goto token_parse_error;
            }

            fd_t fd = open(tokens[i + 1]);
            if (fd == ERR)
            {
                printf("error: unable to open %s (%s)\n", tokens[i + 1], strerror(errno));
                goto token_parse_error;
            }

            cmd_t* cmd = &pipeline->cmds[currentCmd];
            if (cmd->closeStdout)
            {
                close(cmd->stdout);
            }
            cmd->stdout = fd;
            cmd->closeStdout = true;

            i++; // Skip file
        }
        else if (strcmp(tokens[i], ">>") == 0)
        {
            // TODO: Implement this
            printf("error: not implemented");
            goto token_parse_error;
        }
        else if (strcmp(tokens[i], "2>") == 0)
        {
            // TODO: Implement this
            printf("error: not implemented");
            goto token_parse_error;
        }
        else
        {
            currentArgv[currentArg++] = tokens[i];
        }
    }

    if (currentArg > 0)
    {
        currentArgv[currentArg] = NULL;
        pipeline->cmds[currentCmd].argv = currentArgv;
        pipeline->cmds[currentCmd].argc = currentArg;
        currentCmd++;
    }
    else
    {
        free(currentArgv);
    }

    pipeline->amount = currentCmd;
    free(tokens);
    return 0;

token_parse_error:
    free(currentArgv);
    for (uint64_t j = 0; j < currentCmd; j++)
    {
        if (pipeline->cmds[j].closeStdin)
        {
            close(pipeline->cmds[j].stdin);
        }
        if (pipeline->cmds[j].closeStdout)
        {
            close(pipeline->cmds[j].stdout);
        }
        if (pipeline->cmds[j].closeStderr)
        {
            close(pipeline->cmds[j].stderr);
        }
        free(pipeline->cmds[j].argv);
    }
    free(pipeline->cmds);
    free(tokens);
    return ERR;
}

void pipeline_deinit(pipeline_t* pipeline)
{
    for (uint64_t j = 0; j < pipeline->amount; j++)
    {
        if (pipeline->cmds[j].closeStdin)
        {
            close(pipeline->cmds[j].stdin);
        }
        if (pipeline->cmds[j].closeStdout)
        {
            close(pipeline->cmds[j].stdout);
        }
        if (pipeline->cmds[j].closeStderr)
        {
            close(pipeline->cmds[j].stderr);
        }
        free(pipeline->cmds[j].argv);
    }
    if (pipeline->cmds != NULL)
    {
        free(pipeline->cmds);
    }
}

static pid_t pipeline_execute_cmd(cmd_t* cmd)
{
    pid_t result;

    fd_t originalStdin = dup(STDIN_FILENO);
    fd_t originalStdout = dup(STDOUT_FILENO);
    fd_t originalStderr = dup(STDERR_FILENO);

    if (cmd->stdin != STDIN_FILENO)
    {
        dup2(cmd->stdin, STDIN_FILENO);
    }
    if (cmd->stdout != STDOUT_FILENO)
    {
        dup2(cmd->stdout, STDOUT_FILENO);
    }
    if (cmd->stderr != STDERR_FILENO)
    {
        dup2(cmd->stderr, STDERR_FILENO);
    }

    spawn_fd_t fds[] = {
        {.child = STDIN_FILENO, .parent = STDIN_FILENO},
        {.child = STDOUT_FILENO, .parent = STDOUT_FILENO},
        {.child = STDERR_FILENO, .parent = originalStderr},
        SPAWN_FD_END,
    };
    const char** argv = cmd->argv;
    uint64_t argc = 0;
    while (argv[argc] != NULL)
    {
        argc++;
    }

    if (builtin_exists(argv[0]))
    {
        builtin_execute(argc, argv);
        result = ERR;
    }
    else if (argv[0][0] == '.' && argv[0][0] == '/')
    {
        stat_t info;
        if (stat(argv[0], &info) != ERR && info.type == STAT_FILE)
        {
            result = spawn(argv, fds);
        }
        else
        {
            printf("error: %s not found\n", argv[0]);
            result = ERR;
        }
    }
    else
    {
        bool found = false;
        for (uint64_t j = 0; j < sizeof(lookupDirs) / sizeof(lookupDirs[0]); j++)
        {
            if (strlen(lookupDirs[j]) + strlen(argv[0]) + 1 >= MAX_PATH)
            {
                continue;
            }

            char path[MAX_PATH];
            sprintf(path, "%s/%s", lookupDirs[j], argv[0]);

            stat_t info;
            if (stat(path, &info) != ERR && info.type == STAT_FILE)
            {
                const char* temp = argv[0];
                argv[0] = path;
                result = spawn(argv, fds);
                argv[0] = temp;
                found = true;
                break;
            }
        }

        if (!found)
        {
            fprintf(stderr, "shell: %s not found\n", argv[0]);
            result = ERR;
        }
    }

    dup2(originalStdin, STDIN_FILENO);
    dup2(originalStdout, STDOUT_FILENO);
    dup2(originalStderr, STDERR_FILENO);
    close(originalStdin);
    close(originalStdout);
    close(originalStderr);

    if (cmd->closeStdin)
    {
        close(cmd->stdin);
    }
    if (cmd->closeStdout)
    {
        close(cmd->stdout);
    }
    if (cmd->closeStderr)
    {
        close(cmd->stderr);
    }

    return result;
}

void pipeline_execute(pipeline_t* pipeline)
{
    if (pipeline->amount == 0)
    {
        return;
    }

    pid_t* pids = malloc(sizeof(pid_t) * pipeline->amount);

    for (uint64_t i = 0; i < pipeline->amount; i++)
    {
        pids[i] = pipeline_execute_cmd(&pipeline->cmds[i]);
    }

    for (uint64_t i = 0; i < pipeline->amount; i++)
    {
        if (pids[i] != ERR)
        {
            fd_t child = openf("sys:/proc/%d/ctl", pids[i]);
            writef(child, "wait");
            close(child);
        }
    }

    free(pids);
}
