#pragma once

#include <stdint.h>

typedef struct ListEntry
{
    void* data;
    struct ListEntry* prev;
    struct ListEntry* next;
} ListEntry;

typedef struct
{
    uint64_t length;
    ListEntry* first;
    ListEntry* last;
} List;

List* list_new();

void list_push(List* list, void* data);

void list_erase(List* list, ListEntry* entry);

void list_insert_after(List* list, ListEntry* entry, void* data);

void list_free(List* list);