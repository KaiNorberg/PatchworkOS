#include "interactive.h"
#include "ansi.h"
#include "pipeline.h"
#include "history.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/io.h>
#include <sys/kbd.h>
#include <sys/proc.h>

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

typedef struct
{
    ansi_t ansi;
    history_t history;
    int status;
    char buffer[MAX_PATH];
    uint64_t pos;
} interactive_state_t;

static uint64_t interactive_execute_command(interactive_state_t* state)
{
    if (state->pos == 0)
    {
        return 0;
    }

    history_push(&state->history, state->buffer);

    pipeline_t pipeline;
    if (pipeline_init(&pipeline, state->buffer) == ERR)
    {
        printf("shell: failed to initialize pipeline (%s)\n", strerror(errno));
        state->status = EXIT_FAILURE;
        return ERR;
    }

    if (pipeline_execute(&pipeline) == ERR)
    {
        pipeline_deinit(&pipeline);
        return 0; // This is fine
    }

    state->status = pipeline.status;
    pipeline_deinit(&pipeline);
    return 0;
}

static uint64_t interactive_handle_ansi(interactive_state_t* state, ansi_result_t* result)
{
    if (result->type == ANSI_PRINTABLE)
    {
        if (state->pos >= MAX_PATH - 1)
        {
            return 0;
        }
        state->buffer[state->pos++] = result->printable;
        printf("%c", result->printable);
        fflush(stdout);
        return 0;
    }

    switch (result->type)
    {
    case ANSI_BACKSPACE:
        if (state->pos == 0)
        {
            break;
        }
        memmove(&state->buffer[state->pos - 1], &state->buffer[state->pos], MAX_PATH - state->pos + 1);
        state->buffer[MAX_PATH - 1] = '\0';
        state->pos--;
        // Move left, save cursor, print rest of line, clear to end, restore cursor
        printf("\033[1D\033[s%s\033[K\033[u", &state->buffer[state->pos]);
        fflush(stdout);
        break;
    case ANSI_NEWLINE:
        printf("\n");
        if (interactive_execute_command(state) == ERR)
        {
            return ERR;
        }
        memset(state->buffer, 0, MAX_PATH);
        state->pos = 0;
        interactive_prompt();
        break;
    case ANSI_TAB:
        // TODO: Ignore tabs for now
        break;
    case ANSI_ARROW_UP:
    {
        const char* previous = history_previous(&state->history);
        if (previous == NULL)
        {
            break;
        }
        while (state->pos > 0) // Cant use \r because of prompt
        {
            state->pos--;
            printf("\033[D");
        }
        printf("\033[K");
        strncpy(state->buffer, previous, MAX_PATH - 1);
        state->buffer[MAX_PATH - 1] = '\0';
        state->pos = strlen(state->buffer);
        printf("%s", state->buffer);
        fflush(stdout);
    }
    break;
    case ANSI_ARROW_DOWN:
    {
        const char* next = history_next(&state->history);
        while (state->pos > 0) // Cant use \r because of prompt
        {
            state->pos--;
            printf("\033[D");
        }
        printf("\033[K");
        if (next == NULL)
        {
            memset(state->buffer, 0, MAX_PATH);
            state->pos = 0;
            fflush(stdout);
            break;
        }
        strncpy(state->buffer, next, MAX_PATH - 1);
        state->buffer[MAX_PATH - 1] = '\0';
        state->pos = strlen(state->buffer);
        printf("%s", state->buffer);
        fflush(stdout);
    }
    break;
    case ANSI_ARROW_RIGHT:
        if (state->pos < strlen(state->buffer))
        {
            state->pos++;
            printf("\033[C");
            fflush(stdout);
        }
        break;
    case ANSI_ARROW_LEFT:
        if (state->pos > 0)
        {
            state->pos--;
            printf("\033[D");
            fflush(stdout);
        }
        break;
    case ANSI_HOME:
        while (state->pos > 0)
        {
            state->pos--;
            printf("\033[D");
        }
        fflush(stdout);
        break;
    case ANSI_END:
        while (state->pos < strlen(state->buffer))
        {
            state->pos++;
            printf("\033[C");
        }
        fflush(stdout);
        break;
    default:
        break;
    }

    return 0;
}

static uint64_t interactive_handle_input(interactive_state_t* state, const char* input, uint64_t length)
{
    for (uint64_t i = 0; i < length; i++)
    {
        ansi_result_t result;
        if (ansi_parse(&state->ansi, input[i], &result) == ERR)
        {
            printf("shell: failed to parse ansi sequence (%s)\n", strerror(errno));
            continue;
        }

        if (result.type == ANSI_STILL_PARSING)
        {
            continue;
        }

        if (interactive_handle_ansi(state, &result) == ERR)
        {
            return ERR;
        }
    }

    return 0;
}

int interactive_shell(void)
{
    printf("Welcome to the PatchworkOS Shell!\n");
    printf("Type \033[92mhelp\033[m for information on how to use the shell.\n");

    interactive_prompt();

    interactive_state_t state;
    ansi_init(&state.ansi);
    history_init(&state.history);
    state.status = 0;
    memset(state.buffer, 0, MAX_PATH);
    state.pos = 0;

    while (true)
    {
        char buffer[MAX_PATH];
        uint64_t readCount = read(STDIN_FILENO, buffer, MAX_PATH);
        if (readCount == ERR)
        {
            printf("shell: failed to read input (%s)\n", strerror(errno));
            return EXIT_FAILURE;
        }

        if (interactive_handle_input(&state, buffer, readCount) == ERR)
        {
            history_deinit(&state.history);
            return state.status;
        }
    }
}
