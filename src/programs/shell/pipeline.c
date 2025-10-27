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
    pipeline->status = 0;
    if (open2("/dev/pipe/new", pipeline->globalStdin) == ERR)
    {
        free(pipeline->cmds);
        free(tokens);
        return ERR;
    }

    for (uint64_t i = 0; i < tokenAmount; i++)
    {
        cmd_t* cmd = &pipeline->cmds[i];
        cmd->argv = NULL;
        cmd->argc = 0;
        cmd->stdin = pipeline->globalStdin[PIPE_READ];
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

        if (pipeline->cmds[j].argv != NULL)
        {
            for (uint64_t k = 0; pipeline->cmds[j].argv[k] != NULL; k++)
            {
                free((void*)pipeline->cmds[j].argv[k]);
            }
            free(pipeline->cmds[j].argv);
        }
    }
    close(pipeline->globalStdin[PIPE_READ]);
    close(pipeline->globalStdin[PIPE_WRITE]);
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
    close(pipeline->globalStdin[PIPE_READ]);
    close(pipeline->globalStdin[PIPE_WRITE]);
    if (pipeline->cmds != NULL)
    {
        free(pipeline->cmds);
    }
}

static uint64_t pipeline_execute_builtin(cmd_t* cmd)
{
    // The price of not having fork()

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

    uint64_t result = builtin_execute(cmd->argc, cmd->argv);
    if (dup2(originalStdin, STDIN_FILENO) == ERR || dup2(originalStdout, STDOUT_FILENO) == ERR ||
        dup2(originalStderr, STDERR_FILENO) == ERR)
    {
        result = ERR;
    }

    close(originalStdin);
    close(originalStdout);
    close(originalStderr);
    return result;
}

static pid_t pipeline_execute_cmd(cmd_t* cmd)
{
    pid_t result = ERR;

    spawn_fd_t fds[] = {
        {.child = STDIN_FILENO, .parent = cmd->stdin},
        {.child = STDOUT_FILENO, .parent = cmd->stdout},
        {.child = STDERR_FILENO, .parent = cmd->stderr},
        SPAWN_FD_END,
    };

    const char** argv = cmd->argv;
    uint64_t argc = cmd->argc;
    if (builtin_exists(argv[0]))
    {
        if (pipeline_execute_builtin(cmd) == ERR)
        {
            result = ERR;
        }
        else
        {
            result = 0;
        }
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

    pollfd_t* fds = calloc(pipeline->amount + 1, sizeof(pollfd_t));
    if (fds == NULL)
    {
        return ERR;
    }
    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;
    uint64_t fdCount = 1;

    pid_t* pids = malloc(sizeof(pid_t) * pipeline->amount);
    if (pids == NULL)
    {
        free(fds);
        return ERR;
    }

    uint64_t result = 0;
    pipeline->status = 0;

    for (uint64_t i = 0; i < pipeline->amount; i++)
    {
        pid_t pid = pipeline_execute_cmd(&pipeline->cmds[i]);
        if (pid == ERR)
        {
            pipeline->status = 1;
            result = ERR;
            goto cleanup;
        }
        if (pid == 0)
        {
            if (i == pipeline->amount - 1)
            {
                pipeline->status = 0;
            }
            continue;
        }

        pids[fdCount - 1] = pid;

        fds[fdCount].fd = openf("/proc/%d/wait", pid);
        if (fds[fdCount].fd == ERR)
        {
            result = ERR;
            goto cleanup;
        }
        fds[fdCount].events = POLLIN;
        fdCount++;
    }

    if (fdCount == 1)
    {
        // All commands were builtins
        goto cleanup;
    }

    while (true)
    {
        uint64_t ready = poll(fds, fdCount, CLOCKS_NEVER);
        if (ready == ERR)
        {
            result = ERR;
            goto cleanup;
        }

        for (uint64_t i = 0; i < fdCount; i++)
        {
            if (fds[i].revents & POLLERR)
            {
                result = ERR;
                goto cleanup;
            }
        }

        if (fds[0].revents & POLLIN)
        {
            char buffer[MAX_PATH];
            uint64_t bytesRead = read(STDIN_FILENO, buffer, sizeof(buffer));
            if (bytesRead == ERR)
            {
                result = ERR;
                goto cleanup;
            }

            for (uint64_t i = 0; i < bytesRead; i++)
            {
                if (buffer[i] != '\003') // Ctrl-C
                {
                    continue;
                }

                printf("^C\n");
                for (uint64_t j = 1; j < fdCount; j++)
                {
                    fd_t note = openf("/proc/%d/note", pids[j - 1]);
                    if (note == ERR)
                    {
                        continue;
                    }

                    writef(note, "kill");
                    close(note);
                }

                break;
            }

            if (write(pipeline->globalStdin[PIPE_WRITE], buffer, bytesRead) == ERR)
            {
                result = ERR;
                goto cleanup;
            }
        }

        bool allExited = true;
        for (uint64_t i = 1; i < fdCount; i++)
        {
            if (fds[i].revents & POLLIN)
            {
                char buffer[64];
                uint64_t readCount = read(fds[i].fd, buffer, sizeof(buffer) - 1);
                if (readCount == ERR)
                {
                    result = ERR;
                    goto cleanup;
                }
                buffer[readCount] = '\0';

                int exitStatus = atoi(buffer);
                if (i == fdCount - 1)
                {
                    pipeline->status = exitStatus;
                }
            }
            else
            {
                allExited = false;
            }
        }

        if (allExited)
        {
            break;
        }
    }

cleanup:
    for (uint64_t i = 1; i < fdCount; i++)
    {
        close(fds[i].fd);
    }
    free(fds);
    free(pids);
    return result;
}
