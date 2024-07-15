#pragma once

#include "_AUX/NULL.h"

#include <stdbool.h>

// Must be placed at the top of a struct.
typedef struct list_entry
{
    struct list_entry* prev;
    struct list_entry* next;
} list_entry_t;

typedef struct
{
    list_entry_t head;
} list_t;

#define LIST_FOR_EACH(elem, list) \
    for ((elem) = (typeof(elem))((list)->head.next); (elem) != (typeof(elem))(list); \
         (elem) = (typeof(elem))(((list_entry_t*)(elem))->next))

#define LIST_FOR_EACH_SAFE(elem, temp, list) \
    for ((elem) = (typeof(elem))((list)->head.next), (temp) = (typeof(elem))(((list_entry_t*)(elem))->next); \
         (elem) != (typeof(elem))(list); (elem) = (temp), (temp) = (typeof(elem))(((list_entry_t*)(elem))->next))

#define LIST_FOR_EACH_REVERSE(elem, list) \
    for ((elem) = (typeof(elem))((list)->head.prev); (elem) != (typeof(elem))(list); \
         (elem) = (typeof(elem))(((list_entry_t*)(elem))->prev))

#define LIST_FOR_EACH_FROM(elem, start, list) \
    for ((elem) = (typeof(elem))(start); (elem) != (typeof(elem))(list); (elem) = (typeof(elem))(((list_entry_t*)(elem))->next))

#define LIST_FOR_EACH_FROM_REVERSE(elem, start, list) \
    for ((elem) = (typeof(elem))(start); (elem) != (typeof(elem))(list); (elem) = (typeof(elem))(((list_entry_t*)(elem))->prev))

static inline void list_entry_init(list_entry_t* entry)
{
    entry->next = entry;
    entry->prev = entry;
}

static inline void list_init(list_t* list)
{
    list_entry_init(&list->head);
}

static inline bool list_empty(list_t* list)
{
    return list->head.next == &list->head;
}

static inline void list_add(list_entry_t* prev, list_entry_t* next, list_entry_t* elem)
{
    next->prev = elem;
    elem->next = next;
    elem->prev = prev;
    prev->next = elem;
}

static inline void list_append(list_entry_t* prev, void* elem)
{
    list_add(prev, prev->next, elem);
}

static inline void list_prepend(list_entry_t* head, void* elem)
{
    list_add(head->prev, head, elem);
}

static inline void list_remove(void* elem)
{
    list_entry_t* entry = (list_entry_t*)elem;
    entry->prev->next = entry->next;
    entry->next->prev = entry->prev;
    list_entry_init(entry);
}

static inline void list_push(list_t* list, void* elem)
{
    list_add(list->head.prev, &list->head, elem);
}

static inline void* list_pop(list_t* list)
{
    if (list_empty(list))
    {
        return NULL;
    }

    list_entry_t* elem = list->head.next;
    list_remove(elem);
    return elem;
}

static inline void* list_first(list_t* list)
{
    if (list_empty(list))
    {
        return NULL;
    }
    return list->head.next;
}
