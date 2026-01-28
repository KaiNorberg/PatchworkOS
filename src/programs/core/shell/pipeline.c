#include "pipeline.h"
#include "builtin.h"

#include <_libstd/MAX_PATH.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/argsplit.h>
#include <sys/fs.h>
#include <sys/proc.h>

status_t pipeline_init(pipeline_t* pipeline, const char* cmdline, fd_t stdin, fd_t stdout, fd_t stderr)
{
    uint64_t tokenAmount;
    const char** tokens = argsplit(cmdline, UINT64_MAX, &tokenAmount);
    if (tokens == NULL)
    {
        return ERR(USER, NOMEM);
    }

    if (tokenAmount == 0)
    {
        pipeline->cmds = NULL;
        pipeline->capacity = tokenAmount;
        pipeline->amount = 0;
        free(tokens);
        return OK;
    }

    pipeline->cmds = malloc(sizeof(cmd_t) * tokenAmount);
    if (pipeline->cmds == NULL)
    {
        free(tokens);
        return ERR(USER, NOMEM);
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
        cmd->pid = PFAIL;
    }

    uint64_t currentCmd = 0;
    uint64_t currentArg = 0;
    const char** currentArgv = malloc(sizeof(char*) * (tokenAmount + 1));
    if (currentArgv == NULL)
    {
        free(pipeline->cmds);
        free(tokens);
        return ERR(USER, NOMEM);
    }

    for (uint64_t i = 0; i < tokenAmount; i++)
    {
        if (strcmp(tokens[i], "|") == 0)
        {
            if (currentArg == 0)
            {
                printf("shell: empty command in pipeline\n");
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
            status_t status = open2("/dev/pipe/new", pipe);
            if (IS_ERR(status))
            {
                printf("shell: unable to open pipe (%s)\n", codetostr(ST_CODE(status)));
                goto token_parse_error;
            }

            cmd->stdout = pipe[PIPE_WRITE];
            cmd->shouldCloseStdout = true;

            currentCmd++;
            currentArg = 0;
            currentArgv = malloc(sizeof(char*) * (tokenAmount + 1));
            if (currentArgv == NULL)
            {
                printf("shell: out of memory\n");
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
                printf("shell: missing filename after <\n");
                goto token_parse_error;
            }

            fd_t fd;
            status_t status = open(&fd, tokens[i + 1]);
            if (IS_ERR(status))
            {
                printf("shell: unable to open %s (%s)\n", tokens[i + 1], codetostr(ST_CODE(status)));
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
                printf("shell: missing filename after >\n");
                goto token_parse_error;
            }

            fd_t fd;
            status_t status = open(&fd, tokens[i + 1]);
            if (IS_ERR(status))
            {
                printf("shell: unable to open %s (%s)\n", tokens[i + 1], codetostr(ST_CODE(status)));
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
                printf("shell: missing filename after 2>\n");
                goto token_parse_error;
            }

            fd_t fd;
            status_t status = open(&fd, tokens[i + 1]);
            if (IS_ERR(status))
            {
                printf("shell: unable to open %s (%s)\n", tokens[i + 1], codetostr(ST_CODE(status)));
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
                printf("shell: out of memory\n");
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
            printf("shell: pipeline ends with empty command\n");
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
    return OK;

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
    return ERR(USER, INVAL);
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
    pid_t result = PFAIL;

    fd_t originalStdin = FD_NONE;
    if (IS_ERR(dup(STDIN_FILENO, &originalStdin)))
    {
        return PFAIL;
    }
    fd_t originalStdout = FD_NONE;
    if (IS_ERR(dup(STDOUT_FILENO, &originalStdout)))
    {
        close(originalStdin);
        return PFAIL;
    }
    fd_t originalStderr = FD_NONE;
    if (IS_ERR(dup(STDERR_FILENO, &originalStderr)))
    {
        close(originalStdin);
        close(originalStdout);
        return PFAIL;
    }

    fd_t stdinTarget = STDIN_FILENO;
    fd_t stdoutTarget = STDOUT_FILENO;
    fd_t stderrTarget = STDERR_FILENO;
    if (IS_ERR(dup(cmd->stdin, &stdinTarget)) || IS_ERR(dup(cmd->stdout, &stdoutTarget)) ||
        IS_ERR(dup(cmd->stderr, &stderrTarget)))
    {
        close(originalStdin);
        close(originalStdout);
        close(originalStderr);
        return PFAIL;
    }

    const char** argv = cmd->argv;
    uint64_t argc = cmd->argc;
    if (builtin_exists(argv[0]))
    {
        if (builtin_execute(cmd->argc, cmd->argv) == PFAIL)
        {
            result = PFAIL;
        }
        else
        {
            result = 0;
        }
    }
    else if (strchr(argv[0], '/') != NULL)
    {
        stat_t info;
        if (IS_OK(stat(argv[0], &info)) && info.type != VDIR)
        {
            if (IS_ERR(spawn(argv, SPAWN_STDIO_FDS, &result)))
            {
                result = PFAIL;
            }
        }
        else
        {
            printf("shell: %s not found\n", argv[0]);
            result = PFAIL;
        }
    }
    else
    {
        bool isFound = false;
        char* pathEnv = NULL;
        if (IS_ERR(readfiles(&pathEnv, "/proc/self/env/PATH")))
        {
            pathEnv = NULL;
        }
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
                    if (IS_OK(stat(path, &info)) && info.type != VDIR)
                    {
                        const char* newArgv[argc + 1];
                        newArgv[0] = path;
                        for (uint64_t k = 1; k < argc; k++)
                        {
                            newArgv[k] = argv[k];
                        }
                        newArgv[argc] = NULL;
                        if (IS_ERR(spawn(newArgv, SPAWN_STDIO_FDS, &result)))
                        {
                            result = PFAIL;
                        }
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
            result = PFAIL;
        }
    }

    stdinTarget = STDIN_FILENO;
    stdoutTarget = STDOUT_FILENO;
    stderrTarget = STDERR_FILENO;
    if (IS_ERR(dup(originalStdin, &stdinTarget)) || IS_ERR(dup(originalStdout, &stdoutTarget)) ||
        IS_ERR(dup(originalStderr, &stderrTarget)))
    {
        result = PFAIL;
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

        if (cmd->pid == PFAIL)
        {
            strcpy(pipeline->status, "-1");
            continue;
        }

        if (cmd->pid == 0)
        {
            continue;
        }

        fd_t wait;
        if (IS_ERR(open(&wait, F("/proc/%llu/wait", cmd->pid))))
        {
            strcpy(pipeline->status, "-1");
            continue;
        }

        memset(pipeline->status, 0, sizeof(pipeline->status));
        size_t readCount;
        status_t st = RETRY_ON_CODE(read(wait, pipeline->status, sizeof(pipeline->status), &readCount), INTR);
        close(wait);
        if (IS_ERR(st))
        {
            strcpy(pipeline->status, "-1");
            continue;
        }
    }
}
