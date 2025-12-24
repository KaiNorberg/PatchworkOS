#include "pipeline.h"
#include "builtin.h"

#include <_internal/MAX_PATH.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/argsplit.h>
#include <sys/io.h>
#include <sys/proc.h>

uint64_t pipeline_init(pipeline_t* pipeline, const char* cmdline, fd_t stdin, fd_t stdout, fd_t stderr)
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
    if (pipeline->cmds == NULL)
    {
        free(tokens);
        return ERR;
    }

    pipeline->capacity = tokenAmount;
    pipeline->amount = 0;
    pipeline->status[0] = '\0';

    for (uint64_t i = 0; i < tokenAmount; i++)
    {
        cmd_t* cmd = &pipeline->cmds[i];
        cmd->argv = NULL;
        cmd->argc = 0;
        cmd->stdin = stdin;
        cmd->stdout = stdout;
        cmd->stderr = stderr;
        cmd->shouldCloseStdin = false;
        cmd->shouldCloseStdout = false;
        cmd->shouldCloseStderr = false;
        cmd->pid = ERR;
    }

    uint64_t currentCmd = 0;
    uint64_t currentArg = 0;
    const char** currentArgv = malloc(sizeof(char*) * (tokenAmount + 1));
    if (currentArgv == NULL)
    {
        free(pipeline->cmds);
        free(tokens);
        return ERR;
    }

    for (uint64_t i = 0; i < tokenAmount; i++)
    {
        if (strcmp(tokens[i], "|") == 0)
        {
            if (currentArg == 0)
            {
                printf("error: empty command in pipeline\n");
                if (pipeline->cmds[currentCmd].shouldCloseStdin)
                {
                    close(pipeline->cmds[currentCmd].stdin);
                    pipeline->cmds[currentCmd].shouldCloseStdin = false;
                }
                goto token_parse_error;
            }

            currentArgv[currentArg] = NULL;

            cmd_t* cmd = &pipeline->cmds[currentCmd];
            cmd->argv = currentArgv;
            cmd->argc = currentArg;

            fd_t pipe[2];
            if (open2("/dev/pipe/new", pipe) == ERR)
            {
                printf("error: unable to open pipe (%s)\n", strerror(errno));
                goto token_parse_error;
            }

            cmd->stdout = pipe[PIPE_WRITE];
            cmd->shouldCloseStdout = true;

            currentCmd++;
            currentArg = 0;
            currentArgv = malloc(sizeof(char*) * (tokenAmount + 1));
            if (currentArgv == NULL)
            {
                printf("error: out of memory\n");
                goto token_parse_error;
            }

            cmd_t* nextCmd = &pipeline->cmds[currentCmd];
            nextCmd->stdin = pipe[PIPE_READ];
            nextCmd->shouldCloseStdin = true;
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
            if (cmd->shouldCloseStdin)
            {
                close(cmd->stdin);
            }
            cmd->stdin = fd;
            cmd->shouldCloseStdin = true;

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
            if (cmd->shouldCloseStdout)
            {
                close(cmd->stdout);
            }
            cmd->stdout = fd;
            cmd->shouldCloseStdout = true;

            i++; // Skip file
        }
        else if (strcmp(tokens[i], "2>") == 0)
        {
            if (i + 1 >= tokenAmount)
            {
                printf("error: missing filename after 2>\n");
                goto token_parse_error;
            }

            fd_t fd = open(tokens[i + 1]);
            if (fd == ERR)
            {
                printf("error: unable to open %s (%s)\n", tokens[i + 1], strerror(errno));
                goto token_parse_error;
            }

            cmd_t* cmd = &pipeline->cmds[currentCmd];
            if (cmd->shouldCloseStderr)
            {
                close(cmd->stderr);
            }
            cmd->stderr = fd;
            cmd->shouldCloseStderr = true;

            i++; // Skip file
        }
        else
        {
            currentArgv[currentArg] = strdup(tokens[i]);
            if (currentArgv[currentArg] == NULL)
            {
                printf("error: out of memory\n");
                goto token_parse_error;
            }
            currentArg++;
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
        if (pipeline->amount > 0 && currentCmd > 0)
        {
            printf("error: pipeline ends with empty command\n");
            cmd_t* emptyCmd = &pipeline->cmds[currentCmd];
            if (emptyCmd->shouldCloseStdin)
            {
                close(emptyCmd->stdin);
                emptyCmd->shouldCloseStdin = false;
            }
            goto token_parse_error_no_current_argv;
        }
    }

    pipeline->amount = currentCmd;
    free(tokens);
    return 0;

token_parse_error:
    if (currentArgv != NULL)
    {
        for (uint64_t k = 0; k < currentArg; k++)
        {
            free((void*)currentArgv[k]);
        }
    }
    free(currentArgv);

token_parse_error_no_current_argv:
    for (uint64_t j = 0; j < tokenAmount; j++)
    {
        if (pipeline->cmds[j].shouldCloseStdin)
        {
            close(pipeline->cmds[j].stdin);
        }
        if (pipeline->cmds[j].shouldCloseStdout)
        {
            close(pipeline->cmds[j].stdout);
        }
        if (pipeline->cmds[j].shouldCloseStderr)
        {
            close(pipeline->cmds[j].stderr);
        }

        if (pipeline->cmds[j].argv != NULL)
        {
            for (uint64_t k = 0; pipeline->cmds[j].argv[k] != NULL; k++)
            {
                free((void*)pipeline->cmds[j].argv[k]);
            }
            free(pipeline->cmds[j].argv);
        }
    }
    free(pipeline->cmds);
    free(tokens);
    return ERR;
}

void pipeline_deinit(pipeline_t* pipeline)
{
    for (uint64_t j = 0; j < pipeline->amount; j++)
    {
        if (pipeline->cmds[j].shouldCloseStdin)
        {
            close(pipeline->cmds[j].stdin);
        }
        if (pipeline->cmds[j].shouldCloseStdout)
        {
            close(pipeline->cmds[j].stdout);
        }
        if (pipeline->cmds[j].shouldCloseStderr)
        {
            close(pipeline->cmds[j].stderr);
        }

        if (pipeline->cmds[j].argv != NULL)
        {
            for (uint64_t k = 0; pipeline->cmds[j].argv[k] != NULL; k++)
            {
                free((void*)pipeline->cmds[j].argv[k]);
            }
            free(pipeline->cmds[j].argv);
        }
    }
    if (pipeline->cmds != NULL)
    {
        free(pipeline->cmds);
    }
}

static pid_t pipeline_execute_cmd(cmd_t* cmd)
{
    pid_t result = ERR;

    fd_t originalStdin = dup(STDIN_FILENO);
    if (originalStdin == ERR)
    {
        return ERR;
    }
    fd_t originalStdout = dup(STDOUT_FILENO);
    if (originalStdout == ERR)
    {
        close(originalStdin);
        return ERR;
    }
    fd_t originalStderr = dup(STDERR_FILENO);
    if (originalStderr == ERR)
    {
        close(originalStdin);
        close(originalStdout);
        return ERR;
    }

    if (dup2(cmd->stdin, STDIN_FILENO) == ERR || dup2(cmd->stdout, STDOUT_FILENO) == ERR ||
        dup2(cmd->stderr, STDERR_FILENO) == ERR)
    {
        close(originalStdin);
        close(originalStdout);
        close(originalStderr);
        return ERR;
    }

    const char** argv = cmd->argv;
    uint64_t argc = cmd->argc;
    if (builtin_exists(argv[0]))
    {
        if (builtin_execute(cmd->argc, cmd->argv) == ERR)
        {
            result = ERR;
        }
        else
        {
            result = 0;
        }
    }
    else if (strchr(argv[0], '/') != NULL)
    {
        stat_t info;
        if (stat(argv[0], &info) != ERR && info.type != INODE_DIR)
        {
            result = spawn(argv, SPAWN_STDIO_FDS);
        }
        else
        {
            printf("error: %s not found\n", argv[0]);
            result = ERR;
        }
    }
    else
    {
        bool isFound = false;
        char* pathEnv = sreadfile("/proc/self/env/PATH");
        if (pathEnv == NULL)
        {
            pathEnv = strdup("/bin:/usr/bin");
        }

        if (pathEnv != NULL)
        {
            char* token = strtok(pathEnv, ":");
            while (token != NULL)
            {
                char path[MAX_PATH];
                if (snprintf(path, MAX_PATH, "%s/%s", token, argv[0]) < MAX_PATH)
                {
                    stat_t info;
                    if (stat(path, &info) != ERR && info.type != INODE_DIR)
                    {
                        const char* newArgv[argc + 1];
                        newArgv[0] = path;
                        for (uint64_t k = 1; k < argc; k++)
                        {
                            newArgv[k] = argv[k];
                        }
                        newArgv[argc] = NULL;
                        result = spawn(newArgv, SPAWN_STDIO_FDS);
                        isFound = true;
                        break;
                    }
                }
                token = strtok(NULL, ":");
            }
            free(pathEnv);
        }

        if (!isFound)
        {
            fprintf(stderr, "shell: %s not found\n", argv[0]);
            result = ERR;
        }
    }

    if (dup2(originalStdin, STDIN_FILENO) == ERR || dup2(originalStdout, STDOUT_FILENO) == ERR ||
        dup2(originalStderr, STDERR_FILENO) == ERR)
    {
        result = ERR;
    }

    close(originalStdin);
    close(originalStdout);
    close(originalStderr);

    if (cmd->shouldCloseStdin)
    {
        close(cmd->stdin);
        cmd->shouldCloseStdin = false;
    }
    if (cmd->shouldCloseStdout)
    {
        close(cmd->stdout);
        cmd->shouldCloseStdout = false;
    }
    if (cmd->shouldCloseStderr)
    {
        close(cmd->stderr);
        cmd->shouldCloseStderr = false;
    }

    return result;
}

void pipeline_execute(pipeline_t* pipeline)
{
    for (uint64_t i = 0; i < pipeline->amount; i++)
    {
        pipeline->cmds[i].pid = pipeline_execute_cmd(&pipeline->cmds[i]);
    }
}

void pipeline_wait(pipeline_t* pipeline)
{
    for (uint64_t i = 0; i < pipeline->amount; i++)
    {
        cmd_t* cmd = &pipeline->cmds[i];

        if (cmd->pid == ERR)
        {
            strcpy(pipeline->status, "-1");
            continue;
        }

        if (cmd->pid == 0)
        {
            continue;
        }

        fd_t wait = open(F("/proc/%llu/wait", cmd->pid));
        if (wait == ERR)
        {
            strcpy(pipeline->status, "-1");
            continue;
        }

        memset(pipeline->status, 0, sizeof(pipeline->status));
        uint64_t readCount = RETRY_EINTR(read(wait, pipeline->status, sizeof(pipeline->status)));
        close(wait);
        if (readCount == ERR)
        {
            strcpy(pipeline->status, "-1");
            continue;
        }
    }
}
