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
    "/bin",
    "/usr/bin",
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
    if (pipeline->cmds == NULL)
    {
        free(tokens);
        return ERR;
    }

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
        cmd->shouldCloseStdin = false;
        cmd->shouldCloseStdout = false;
        cmd->shouldCloseStderr = false;
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
                goto token_parse_error;
            }

            currentArgv[currentArg] = NULL;

            cmd_t* cmd = &pipeline->cmds[currentCmd];
            cmd->argv = currentArgv;
            cmd->argc = currentArg;

            fd_t pipe[2];
            if (open2("/dev/pipe", pipe) == ERR)
            {
                printf("error: unable to open pipe (%s)\n", strerror(errno));
                goto token_parse_error;
            }

            cmd->stdout = pipe[PIPE_WRITE];
            cmd->shouldCloseStdout = true;

            currentCmd++;
            currentArg = 0;
            currentArgv = malloc(sizeof(char*) * (tokenAmount + 1));

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
        free(pipeline->cmds[j].argv);
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
    fd_t originalStdout = dup(STDOUT_FILENO);
    fd_t originalStderr = dup(STDERR_FILENO);

    if (originalStdin == ERR || originalStdout == ERR || originalStderr == ERR)
    {
        if (originalStdin != ERR)
        {
            close(originalStdin);
        }
        if (originalStdout != ERR)
        {
            close(originalStdout);
        }
        if (originalStderr != ERR)
        {
            close(originalStderr);
        }
        return ERR;
    }

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
        {.child = STDERR_FILENO, .parent = STDERR_FILENO},
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
        return builtin_execute(argc, argv) == ERR ? ERR : 0;
    }
    else if (argv[0][0] == '.' && argv[0][1] == '/')
    {
        stat_t info;
        if (stat(argv[0], &info) != ERR && info.type != INODE_DIR)
        {
            result = spawn(argv, fds, NULL, NULL);
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
        for (uint64_t j = 0; j < sizeof(lookupDirs) / sizeof(lookupDirs[0]); j++)
        {
            if (strlen(lookupDirs[j]) + strlen(argv[0]) + 1 >= MAX_PATH)
            {
                continue;
            }

            char path[MAX_PATH];
            sprintf(path, "%s/%s", lookupDirs[j], argv[0]);

            stat_t info;
            if (stat(path, &info) != ERR && info.type != INODE_DIR)
            {
                const char* temp = argv[0];
                argv[0] = path;
                result = spawn(argv, fds, NULL, SPAWN_NONE);
                argv[0] = temp;
                isFound = true;
                break;
            }
        }

        if (!isFound)
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

uint64_t pipeline_execute(pipeline_t* pipeline)
{
    if (pipeline->amount == 0)
    {
        return 0;
    }

    pid_t* pids = malloc(sizeof(pid_t) * pipeline->amount);
    if (pids == NULL)
    {
        return ERR;
    }

    for (uint64_t i = 0; i < pipeline->amount; i++)
    {
        pids[i] = pipeline_execute_cmd(&pipeline->cmds[i]);
    }

    bool error = false;
    for (uint64_t i = 0; i < pipeline->amount; i++)
    {
        if (pids[i] == ERR)
        {
            error = true;
            continue;
        }

        // Skip builtin commands
        if (pids[i] == 0)
        {
            continue;
        }

        fd_t status = openf("/proc/%d/status", pids[i]);
        if (status == ERR)
        {
            error = true;
            continue;
        }

        char buf[64];
        uint64_t readCount = read(status, buf, sizeof(buf) - 1);
        close(status);

        if (readCount == ERR)
        {
            error = true;
            continue;
        }

        buf[readCount] = '\0';
        int exitStatus = atoi(buf);
        if (exitStatus != 0)
        {
            error = true;
        }
    }

    free(pids);
    return error ? ERR : 0;
}
