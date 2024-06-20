#pragma once

#include "defs.h"
#include "lock.h"

typedef struct safe_list safe_list_t;
typedef struct safe_list_entry safe_list_entry_t;

// Must be placed at the top of a struct.
typedef struct safe_list_entry
{
    safe_list_t* list;
    safe_list_entry_t* prev;
    safe_list_entry_t* next;
} safe_list_entry_t;

typedef struct safe_list
{
    lock_t lock;
    safe_list_entry_t head;
} safe_list_t;

#define SAFE_LIST_FOR_EACH(elem, list) \
    for ((elem) = (typeof(elem))((list)->head.next); (elem) != (typeof(elem))(list); \
         (elem) = (typeof(elem))(((list_entry_t*)(elem))->next))

// Allows for safely removing elements from the list while iterating over it.
#define SAFE_LIST_FOR_EACH_SAFE(elem, temp, list) \
    for ((elem) = (typeof(elem))((list)->head.next), (temp) = (typeof(elem))(((list_entry_t*)(elem))->next); \
         (elem) != (typeof(elem))(list); (elem) = (temp), (temp) = (typeof(elem))(((list_entry_t*)(elem))->next))

static inline void safe_list_entry_init(safe_list_entry_t* entry)
{
    entry->next = entry;
    entry->prev = entry;
}

static inline void safe_list_init(safe_list_t* list)
{
    lock_init(&list->lock);
    safe_list_entry_init(&list->head);
}

static inline bool safe_list_empty(safe_list_t* list)
{
    LOCK_GUARD(&list->lock);

    return list->head.next == &list->head;
}

static inline void safe_list_append(safe_list_entry_t* head, void* element)
{
    LOCK_GUARD(&head->list->lock);

    safe_list_entry_t* header = (safe_list_entry_t*)element;
    header->next = head->next;
    header->prev = head;
    header->list = head->list;
    head->next->prev = header;
    head->next = header;
}

static inline void safe_list_prepend(safe_list_entry_t* head, void* element)
{
    safe_list_append(head->prev, element);
}

static inline void safe_list_remove(safe_list_t* list, void* element)
{
    LOCK_GUARD(&list->lock);

    safe_list_entry_t* header = (safe_list_entry_t*)element;
    header->next->prev = header->prev;
    header->prev->next = header->next;
    header->next = header;
    header->prev = header;
}

static inline void safe_list_push(safe_list_t* list, void* element)
{
    safe_list_prepend(&list->head, element);
}
