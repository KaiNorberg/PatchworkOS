#pragma once

#include "defs/defs.h"

//Must be placed at the top of a struct.
typedef struct ListHead
{
    struct ListHead* prev;
    struct ListHead* next;
} ListHead;

typedef struct
{
    ListHead head;
} List;

#define LIST_FOR_EACH(elem, list) \
    for ((elem) = (typeof(elem))((list)->head.next); \
        (elem) != (typeof(elem))(list); \
        (elem) = (typeof(elem))(((ListHead*)(elem))->next))

//Allows for safely removing elements from the list while iterating over it.
#define LIST_FOR_EACH_SAFE(elem, temp, list) \
    for ((elem) = (typeof(elem))((list)->head.next), \
            (temp) = (typeof(elem))(((ListHead*)(elem))->next); \
        (elem) != (typeof(elem))(list); \
        (elem) = (temp), \
            (temp) = (typeof(elem))(((ListHead*)(elem))->next))

static inline void list_head_init(ListHead* head)
{
    head->next = head;
    head->prev = head;
}

static inline void list_init(List* list)
{
    list_head_init(&list->head);
}

static inline bool list_empty(List* list)
{
    return list->head.next == &list->head;
}

//Add an element after "head".
static inline void list_append(ListHead* head, void* element)
{
    ListHead* header = (ListHead*)element;

    header->next = head->next;
    header->prev = head;
    head->next->prev = header;
    head->next = header;
}

//Add an element before "head".
static inline void list_prepend(ListHead* head, void* element)
{
    ListHead* header = (ListHead*)element;

    header->next = head;
    header->prev = head->prev;
    head->prev->next = header;
    head->prev = header;
}

static inline void list_remove(void* element)
{
    ListHead* header = (ListHead*)element;

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

    ListHead* element = list->head.prev;
    list_remove(element);
    return element;
}