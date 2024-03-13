#include "list.h"

#include "heap/heap.h"

List* list_new(void)
{
    List* list = kmalloc(sizeof(List));

    list->length = 0;
    list->first = 0;
    list->last = 0;

    return list;
}

void list_push(List* list, void* data)
{
    ListEntry* entry = kmalloc(sizeof(ListEntry));

    entry->data = data;

    if (list->first == 0)
    {
        entry->next = 0;
        entry->prev = 0;

        list->first = entry;
        list->last = entry;
    }
    else
    {
        entry->next = 0;
        entry->prev = list->last;

        list->last->next = entry;
        list->last = entry;
    }
}

void list_erase(List* list, ListEntry* entry)
{
    if (list->first == entry)
    {
        list->first = list->first->next;
    }
    if (list->last == entry)
    {
        list->last = list->last->prev;
    }

    if (entry->prev != 0)
    {
        entry->prev->next = entry->next;
    }
    if (entry->next != 0)
    {
        entry->next->prev = entry->prev;
    }

    kfree(entry);
}

void list_insert_after(List* list, ListEntry* entry, void* data)
{    
    ListEntry* newEntry = kmalloc(sizeof(ListEntry));

    newEntry->data = data;
    newEntry->next = 0;
    newEntry->prev = entry;

    if (list->last == entry)
    {
        list->last->next = newEntry;
        list->last = newEntry;
    }
    else if (entry->next != 0)
    {
        newEntry->next = entry->next;

        entry->next->prev = newEntry;
    }

    entry->next = newEntry;
}

void list_free(List* list)
{
    ListEntry* entry = list->first;
    while (entry != 0)
    {
        ListEntry* nextEntry = entry->next;

        kfree(entry);

        entry = nextEntry;
    }

    kfree(list);
}