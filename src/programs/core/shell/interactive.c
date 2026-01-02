#include "interactive.h"
#include "ansi.h"
#include "history.h"
#include "pipeline.h"

#include <errno.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/defs.h>
#include <sys/io.h>
#include <sys/kbd.h>
#include <sys/proc.h>
#include <threads.h>

static ansi_t ansi;
static history_t history;
static char buffer[MAX_PATH];
static uint64_t pos;

static void interactive_sigint_handler(int sig)
{
    UNUSED(sig);

    // Do nothing, only child processes should be interrupted.
}

static void interactive_prompt(void)
{
    char cwd[MAX_PATH] = {0};
    if (readfile("/proc/self/cwd", cwd, MAX_PATH - 1, 0) == ERR)
    {
        strcpy(cwd, "?");
    }
    printf("\n\033[32m%s\n\033[92m>\033[m ", cwd);
    fflush(stdout);
}

static void interactive_execute(void)
{
    if (pos == 0)
    {
        interactive_prompt();
        return;
    }

    history_push(&history, buffer);

    pipeline_t pipeline;
    if (pipeline_init(&pipeline, buffer, STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO) == ERR)
    {
        interactive_prompt();
        return;
    }

    pipeline_execute(&pipeline);
    pipeline_wait(&pipeline);

    if (strlen(pipeline.status) > 0 && strcmp(pipeline.status, "0") != 0 && strcmp(pipeline.status, "-1") != 0)
    {
        int status;
        if (sscanf(pipeline.status, "%d", &status) == 1)
        {
            if (status > 0 && status < EMAX)
            {
                printf("shell: %s\n", strerror(status));
            }
            else if (status < 0 && -status > 0 && -status < EMAX)
            {
                printf("shell: %s\n", strerror(-status));
            }
            else
            {
                printf("shell: %llu\n", status);
            }
        }
        else
        {
            printf("shell: %s\n", pipeline.status);
        }
    }

    pipeline_deinit(&pipeline);

    interactive_prompt();
}

static void interactive_handle_ansi(ansi_result_t* result)
{
    if (result->type == ANSI_PRINTABLE)
    {
        if (pos >= MAX_PATH - 1)
        {
            return;
        }
        memmove(&buffer[pos + 1], &buffer[pos], MAX_PATH - pos - 1);
        buffer[pos++] = result->printable;
        printf("%c\033[s%s\033[u", result->printable, &buffer[pos]);
        fflush(stdout);
        return;
    }

    switch (result->type)
    {
    case ANSI_BACKSPACE:
        if (pos == 0)
        {
            break;
        }
        memmove(&buffer[pos - 1], &buffer[pos], MAX_PATH - pos + 1);
        buffer[MAX_PATH - 1] = '\0';
        pos--;
        // Move left, save cursor, print rest of line, clear to end, restore cursor
        printf("\033[1D\033[s%s\033[K\033[u", &buffer[pos]);
        fflush(stdout);
        break;
    case ANSI_NEWLINE:
        printf("\n");
        interactive_execute();
        printf("\033[0m\033[?25h"); // Reset colors and cursor
        fflush(stdout);
        memset(buffer, 0, MAX_PATH);
        pos = 0;
        break;
    case ANSI_TAB:
        /// @todo Implement tab completion
        break;
    case ANSI_ARROW_UP:
    {
        const char* previous = history_previous(&history);
        if (previous == NULL)
        {
            break;
        }
        while (pos > 0) // Cant use \r because of prompt
        {
            pos--;
            printf("\033[D");
        }
        strncpy(buffer, previous, MAX_PATH - 1);
        buffer[MAX_PATH - 1] = '\0';
        pos = strlen(buffer);
        printf("\033[K%s", buffer);
        fflush(stdout);
    }
    break;
    case ANSI_ARROW_DOWN:
    {
        const char* next = history_next(&history);
        while (pos > 0) // Cant use \r because of prompt
        {
            pos--;
            printf("\033[D");
        }
        printf("\033[K");
        if (next == NULL)
        {
            memset(buffer, 0, MAX_PATH);
            pos = 0;
            fflush(stdout);
            break;
        }
        strncpy(buffer, next, MAX_PATH - 1);
        buffer[MAX_PATH - 1] = '\0';
        pos = strlen(buffer);
        printf("%s", buffer);
        fflush(stdout);
    }
    break;
    case ANSI_ARROW_RIGHT:
        if (pos < strlen(buffer))
        {
            pos++;
            printf("\033[C");
            fflush(stdout);
        }
        break;
    case ANSI_ARROW_LEFT:
        if (pos > 0)
        {
            pos--;
            printf("\033[D");
            fflush(stdout);
        }
        break;
    case ANSI_HOME:
        while (pos > 0)
        {
            pos--;
            printf("\033[D");
        }
        fflush(stdout);
        break;
    case ANSI_END:
        while (pos < strlen(buffer))
        {
            pos++;
            printf("\033[C");
        }
        fflush(stdout);
        break;
    case ANSI_CTRL_C:
        break;
    default:
        break;
    }
}

static void interactive_handle_input(const char* input, uint64_t length)
{
    for (uint64_t i = 0; i < length; i++)
    {
        ansi_result_t result;
        if (ansi_parse(&ansi, input[i], &result) == ERR)
        {
            continue;
        }

        if (result.type == ANSI_STILL_PARSING)
        {
            continue;
        }

        interactive_handle_ansi(&result);
    }
}

void interactive_shell(void)
{
    if (signal(SIGINT, interactive_sigint_handler) == SIG_ERR)
    {
        _exit(F("shell: failed to set SIGINT handler (%s)\n", strerror(errno)));
    }

    printf("Welcome to the PatchworkOS Shell!\n");
    printf("Type \033[92mhelp\033[m for information on how to use the shell.\n");

    interactive_prompt();

    ansi_init(&ansi);
    history_init(&history);
    memset(buffer, 0, MAX_PATH);
    pos = 0;

    while (true)
    {
        char buffer[MAX_PATH];
        uint64_t readCount = RETRY_EINTR(read(STDIN_FILENO, buffer, MAX_PATH));
        if (readCount == ERR)
        {
            printf("shell: failed to read input (%s)\n", strerror(errno));
            exit(EXIT_FAILURE);
        }

        interactive_handle_input(buffer, readCount);
    }
}
