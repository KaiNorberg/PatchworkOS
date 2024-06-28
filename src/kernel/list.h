#pragma once

#include "defs.h"

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

static inline void list_append(list_entry_t* head, void* element)
{
    list_entry_t* header = (list_entry_t*)element;

    header->next = head->next;
    header->prev = head;
    head->next->prev = header;
    head->next = header;
}

static inline void list_prepend(list_entry_t* head, void* element)
{
    list_append(head->prev, element);
}

static inline void list_remove(void* element)
{
    list_entry_t* header = (list_entry_t*)element;

    header->next->prev = header->prev;
    header->prev->next = header->next;
    header->next = header;
    header->prev = header;
}

static inline void list_push(list_t* list, void* element)
{
    list_prepend(&list->head, element);
}

static inline void* list_pop(list_t* list)
{
    if (list_empty(list))
    {
        return NULL;
    }

    list_entry_t* element = list->head.prev;
    list_remove(element);
    return element;
}
