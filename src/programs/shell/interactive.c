#include "interactive.h"
#include "ansi.h"
#include "builtin.h"

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

    printf("Executing command: %s\n", state->buffer);
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
        state->pos--;
        memmove(&state->buffer[state->pos], &state->buffer[state->pos + 1], MAX_PATH - state->pos - 1);
        state->buffer[MAX_PATH - 1] = 0;
        printf("\b \b");
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
        // Ignore tabs for now
        break;
    default:
        // Ignore other ANSI sequences for now
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
            return state.status;
        }
    }
}
