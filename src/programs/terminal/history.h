#pragma once

#include <stdint.h>
#include <string.h>

#define HISTORY_MAX_ENTRY 32

typedef struct
{
    char* entries[HISTORY_MAX_ENTRY];
    uint64_t count;
    uint64_t index;
} history_t;

void history_init(history_t* history);

void history_deinit(history_t* history);

void history_push(history_t* history, const char* entry);

const char* history_next(history_t* history);

const char* history_previous(history_t* history);
