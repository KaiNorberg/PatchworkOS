#ifndef _SYS_LIST_H
#define _SYS_LIST_H 1

#include "_AUX/NULL.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct list_entry
{
    struct list_entry* prev;
    struct list_entry* next;
} list_entry_t;

typedef struct
{
    list_entry_t head;
} list_t;

#define LIST_CONTAINER(ptr, type, member) ((type*)((char*)(ptr) - offsetof(type, member)))

#define LIST_CONTAINER_SAFE(ptr, type, member) \
    ({ \
        list_entry_t* entry = ptr; \
        ((entry != NULL) ? LIST_CONTAINER(entry, type, member) : NULL); \
    })

#define LIST_FOR_EACH(elem, list, member) \
    for ((elem) = LIST_CONTAINER((list)->head.next, typeof(*elem), member); &(elem)->member != &((list)->head); \
        (elem) = LIST_CONTAINER((elem)->member.next, typeof(*elem), member))

#define LIST_FOR_EACH_SAFE(elem, temp, list, member) \
    for ((elem) = LIST_CONTAINER((list)->head.next, typeof(*elem), member), \
        (temp) = LIST_CONTAINER((elem)->member.next, typeof(*elem), member); \
        &(elem)->member != &((list)->head); \
        (elem) = (temp), (temp) = LIST_CONTAINER((elem)->member.next, typeof(*elem), member))

#define LIST_FOR_EACH_REVERSE(elem, list, member) \
    for ((elem) = LIST_CONTAINER((list)->head.prev, typeof(*elem), member); &(elem)->member != &((list)->head); \
        (elem) = LIST_CONTAINER((elem)->member.prev, typeof(*elem), member))

#define LIST_FOR_EACH_FROM(elem, start, list, member) \
    for ((elem) = LIST_CONTAINER(start, typeof(*elem), member); &(elem)->member != &((list)->head); \
        (elem) = LIST_CONTAINER((elem)->member.next, typeof(*elem), member))

#define LIST_FOR_EACH_FROM_REVERSE(elem, start, list, member) \
    for ((elem) = LIST_CONTAINER(start, typeof(*elem), member); &(elem)->member != &((list)->head); \
        (elem) = LIST_CONTAINER((elem)->member.prev, typeof(*elem), member))

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

static inline void list_append(list_entry_t* prev, list_entry_t* entry)
{
    list_add(prev, prev->next, entry);
}

static inline void list_prepend(list_entry_t* head, list_entry_t* entry)
{
    list_add(head->prev, head, entry);
}

static inline void list_remove(list_entry_t* entry)
{
    entry->prev->next = entry->next;
    entry->next->prev = entry->prev;
    list_entry_init(entry);
}

static inline void list_push(list_t* list, list_entry_t* entry)
{
    list_add(list->head.prev, &list->head, entry);
}

static inline list_entry_t* list_pop(list_t* list)
{
    if (list_empty(list))
    {
        return NULL;
    }

    list_entry_t* entry = list->head.next;
    list_remove(entry);
    return entry;
}

static inline list_entry_t* list_first(list_t* list)
{
    if (list_empty(list))
    {
        return NULL;
    }
    return list->head.next;
}

#endif
