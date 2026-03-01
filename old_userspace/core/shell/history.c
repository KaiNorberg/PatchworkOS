#include "history.h"

#include <stdlib.h>
#include <string.h>

void history_init(history_t* history)
{
    history->count = 0;
    history->index = 0;
}

void history_deinit(history_t* history)
{
    for (uint64_t i = 0; i < history->count; i++)
    {
        free(history->entries[i]);
    }
}

void history_push(history_t* history, const char* entry)
{
    if (history->count != 0 && strcmp(entry, history->entries[history->count - 1]) == 0)
    {
        history->index = history->count;
        return;
    }

    uint64_t entryLen = strlen(entry);
    if (entryLen == 0)
    {
        history->index = history->count;
        return;
    }

    if (history->count == HISTORY_MAX_ENTRY)
    {
        free(history->entries[0]);
        memmove(history->entries, history->entries + 1, sizeof(char*) * (HISTORY_MAX_ENTRY - 1));
        history->count--;
    }

    history->entries[history->count] = malloc(entryLen + 1);
    strcpy(history->entries[history->count], entry);
    history->count++;
    history->index = history->count;
}

const char* history_next(history_t* history)
{
    if (history->count == 0 || history->index >= history->count - 1)
    {
        history->index = history->count;
        return NULL;
    }

    history->index++;
    return history->entries[history->index];
}

const char* history_previous(history_t* history)
{
    if (history->count == 0 || history->index == 0)
    {
        return NULL;
    }

    history->index--;
    return history->entries[history->index];
}