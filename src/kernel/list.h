#pragma once

#include "defs.h"

// Must be placed at the top of a struct.
typedef struct ListEntry
{
    struct ListEntry* prev;
    struct ListEntry* next;
} ListEntry;

typedef struct
{
    ListEntry head;
} List;

#define LIST_FOR_EACH(elem, list) \
    for ((elem) = (typeof(elem))((list)->head.next); (elem) != (typeof(elem))(list); \
         (elem) = (typeof(elem))(((ListEntry*)(elem))->next))

// Allows for safely removing elements from the list while iterating over it.
#define LIST_FOR_EACH_SAFE(elem, temp, list) \
    for ((elem) = (typeof(elem))((list)->head.next), (temp) = (typeof(elem))(((ListEntry*)(elem))->next); \
         (elem) != (typeof(elem))(list); (elem) = (temp), (temp) = (typeof(elem))(((ListEntry*)(elem))->next))

static inline void list_entry_init(ListEntry* entry)
{
    entry->next = entry;
    entry->prev = entry;
}

static inline void list_init(List* list)
{
    list_entry_init(&list->head);
}

static inline bool list_empty(List* list)
{
    return list->head.next == &list->head;
}

static inline void list_append(ListEntry* head, void* element)
{
    ListEntry* header = (ListEntry*)element;

    header->next = head->next;
    header->prev = head;
    head->next->prev = header;
    head->next = header;
}

static inline void list_prepend(ListEntry* head, void* element)
{
    list_append(head->prev, element);
}

static inline void list_remove(void* element)
{
    ListEntry* header = (ListEntry*)element;

    header->next->prev = header->prev;
    header->prev->next = header->next;
    header->next = header;
    header->prev = header;
}

static inline void list_push(List* list, void* element)
{
    list_prepend(&list->head, element);
}

static inline void* list_pop(List* list)
{
    if (list_empty(list))
    {
        return NULL;
    }

    ListEntry* element = list->head.prev;
    list_remove(element);
    return element;
}