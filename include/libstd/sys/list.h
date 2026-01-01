#ifndef _SYS_LIST_H
#define _SYS_LIST_H 1

#include "_internal/CONTAINER_OF.h"
#include "_internal/NULL.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Doubly linked list header.
 * @ingroup libstd
 * @defgroup libstd_sys_list Doubly linked list
 *
 * The `sys/list.h` header implements a intrusive doubly linked list where the linked list entry structure is stored
 * within each entry instead of each entry having a pointer to each stucture.
 *
 * Given a entry within a structure, the `CONTAINER_OF()` macro can be used to get a pointer to the structure from the
 * list entry pointer.
 *
 * @{
 */

typedef struct list list_t;

/**
 * @brief A entry in a doubly linked list.
 * @struct list_entry_t
 *
 * This structure should be placed within another structure so that the `CONTAINER_OF()` macro can then be used to
 * access the other structure.
 */
typedef struct list_entry
{
    struct list_entry* prev; ///< The previous entry in the list
    struct list_entry* next; ///< The next entry in the list
    list_t* list;            ///< The list this entry belongs to.
} list_entry_t;

/**
 * @brief A doubly linked list.
 *
 * This structure simplifies reasoning around linked lists.
 */
typedef struct list
{
    list_entry_t head; ///< The head of the list, where head::prev is the last entry of the list and head::next is the
                       ///< first entry of the list.
    uint64_t length;   ///< The number of elements in the list (excluding the head).
} list_t;

/**
 * @brief Iterates over a list.
 *
 * @param elem The loop variable, a pointer to the structure containing the list entry.
 * @param list A pointer to the `list_t` structure to iterate over.
 * @param member The name of the `list_entry_t` member within the structure `elem`.
 */
#define LIST_FOR_EACH(elem, list, member) \
    for ((elem) = CONTAINER_OF((list)->head.next, typeof(*elem), member); &(elem)->member != &((list)->head); \
        (elem) = CONTAINER_OF((elem)->member.next, typeof(*elem), member))

/**
 * @brief Safely iterates over a list, allowing for element removal during iteration.
 *
 * The `LIST_FOR_EACH_SAFE()` macro is similar to `LIST_FOR_EACH()` but uses a temporary variable to store the next
 * element, making it safe to remove the current element during iteration.
 *
 * @param elem The loop variable, a pointer to the structure containing the list entry.
 * @param temp A temporary loop variable, a pointer to the structure containing the next list entry.
 * @param list A pointer to the `list_t` structure to iterate over.
 * @param member The name of the `list_entry_t` member within the structure `elem`.
 */
#define LIST_FOR_EACH_SAFE(elem, temp, list, member) \
    for ((elem) = CONTAINER_OF((list)->head.next, typeof(*elem), member), \
        (temp) = CONTAINER_OF((elem)->member.next, typeof(*elem), member); \
        &(elem)->member != &((list)->head); \
        (elem) = (temp), (temp) = CONTAINER_OF((elem)->member.next, typeof(*elem), member))

/**
 * @brief Iterates over a list in reverse.
 *
 * @param elem The loop variable, a pointer to the structure containing the list entry.
 * @param list A pointer to the `list_t` structure to iterate over.
 * @param member The name of the `list_entry_t` member within the structure `elem`.
 */
#define LIST_FOR_EACH_REVERSE(elem, list, member) \
    for ((elem) = CONTAINER_OF((list)->head.prev, typeof(*elem), member); &(elem)->member != &((list)->head); \
        (elem) = CONTAINER_OF((elem)->member.prev, typeof(*elem), member))

/**
 * @brief Iterates over a list starting from a specific element.
 *
 * The `LIST_FOR_EACH_FROM()` macro iterates from a specific element, inclusive, until the end of the list.
 *
 * @param elem The loop variable, a pointer to the structure containing the list entry.
 * @param start A pointer to the `list_entry_t` from which to start iteration.
 * @param list A pointer to the `list_t` structure to iterate over.
 * @param member The name of the `list_entry_t` member within the structure `elem`.
 */
#define LIST_FOR_EACH_FROM(elem, start, list, member) \
    for ((elem) = CONTAINER_OF(start, typeof(*elem), member); &(elem)->member != &((list)->head); \
        (elem) = CONTAINER_OF((elem)->member.next, typeof(*elem), member))

/**
 * @brief Iterates over a list in reverse order starting from a specific element.
 *
 * @param elem The loop variable, a pointer to the structure containing the list entry.
 * @param start A pointer to the `list_entry_t` from which to start reverse iteration.
 * @param list A pointer to the `list_t` structure to iterate over.
 * @param member The name of the `list_entry_t` member within the structure `elem`.
 */
#define LIST_FOR_EACH_FROM_REVERSE(elem, start, list, member) \
    for ((elem) = CONTAINER_OF(start, typeof(*elem), member); &(elem)->member != &((list)->head); \
        (elem) = CONTAINER_OF((elem)->member.prev, typeof(*elem), member))

/**
 * @brief Iterates over a list up to a specific element.
 *
 * The `LIST_FOR_EACH_TO()` macro iterates from the start of the list, inclusive, until a specified element, not
 * inclusive.
 *
 * @param elem The loop variable, a pointer to the structure containing the list entry.
 * @param end A pointer to the `list_entry_t` at which to stop iteration (exclusive).
 * @param list A pointer to the `list_t` structure to iterate over.
 * @param member The name of the `list_entry_t` member within the structure `elem`.
 */
#define LIST_FOR_EACH_TO(elem, end, list, member) \
    for ((elem) = CONTAINER_OF((list)->head.next, typeof(*elem), member); \
        &(elem)->member != &((list)->head) && &(elem)->member != (end); \
        (elem) = CONTAINER_OF((elem)->member.next, typeof(*elem), member))

/**
 * @brief Iterates over a list in reverse order up to a specific element.
 *
 * @param elem The loop variable, a pointer to the structure containing the list entry.
 * @param end A pointer to the `list_entry_t` at which to stop reverse iteration (exclusive).
 * @param list A pointer to the `list_t` structure to iterate over.
 * @param member The name of the `list_entry_t` member within the structure `elem`.
 */
#define LIST_FOR_EACH_TO_REVERSE(elem, end, list, member) \
    for ((elem) = CONTAINER_OF((list)->head.prev, typeof(*elem), member); \
        &(elem)->member != &((list)->head) && &(elem)->member != (end); \
        (elem) = CONTAINER_OF((elem)->member.prev, typeof(*elem), member))

/**
 * @brief Creates a list entry initializer.
 *
 * @param name The name of the entry variable to initialize.
 * @return A `list_entry_t` initializer for the specified entry variable.
 */
#define LIST_ENTRY_CREATE(name) \
    (list_entry_t) \
    { \
        .prev = &(name), .next = &(name), .list = NULL \
    }

/**
 * @brief Creates a list initializer.
 *
 * @param name The name of the list variable to initialize.
 * @return A `list_t` initializer for the specified list variable.
 */
#define LIST_CREATE(name) (list_t){.head = {.prev = &(name).head, .next = &(name).head, .list = &(name)}, .length = 0}

/**
 * @brief Initializes a list entry.
 *
 * @param entry A pointer to the `list_entry_t` to initialize.
 */
static inline void list_entry_init(list_entry_t* entry)
{
    assert(entry != NULL);
    entry->next = entry;
    entry->prev = entry;
    entry->list = NULL;
}

/**
 * @brief Initializes a list.
 *
 * @param list A pointer to the `list_t` to initialize.
 */
static inline void list_init(list_t* list)
{
    assert(list != NULL);
    list_entry_init(&list->head);
    list->head.next->list = list; // Also sets prev
    list->length = 0;
}

/**
 * @brief Check if an entry belongs to a specific list.
 *
 * @param list A pointer to the `list_t` to search in.
 * @param entry A pointer to the `list_entry_t` to search for.
 * @return `true` if the entry is in the list, `false` otherwise.
 */
static inline bool list_contains_entry(list_t* list, list_entry_t* entry)
{
    assert(list != NULL);
    assert(entry != NULL);

    return entry->list == list;
}

/**
 * @brief Checks if a list is empty.
 *
 * @param list A pointer to the `list_t` to check.
 * @return `true` if the list is empty, `false` otherwise.
 */
static inline bool list_is_empty(list_t* list)
{
    assert(list != NULL);

    bool emptyByHead = (list->head.next == &list->head);
    bool emptyByLength = (list->length == 0);

    assert(emptyByHead == emptyByLength);

    return emptyByHead;
}

/**
 * @brief Gets the length of the list.
 *
 * @param list A pointer to the `list_t`.
 * @return The number of elements in the list.
 */
static inline uint64_t list_length(list_t* list)
{
    assert(list != NULL);
    return list->length;
}

/**
 * @brief Adds a new element between two existing list entries.
 *
 * @param list A pointer to the `list_t` that will contain the new element.
 * @param prev A pointer to the list entry that will precede the new element.
 * @param next A pointer to the list entry that will follow the new element.
 * @param elem A pointer to the `list_entry_t` to add.
 */
static inline void list_add(list_t* list, list_entry_t* prev, list_entry_t* next, list_entry_t* entry)
{
    assert(list != NULL);
    assert(prev != NULL);
    assert(next != NULL);
    assert(entry != NULL);
    assert(entry->next == entry && entry->prev == entry);
    assert(prev->next == next && next->prev == prev);
    assert(entry->list == NULL);
    assert(prev->list == list);
    assert(next->list == list);

    next->prev = entry;
    entry->next = next;
    entry->prev = prev;
    prev->next = entry;

    entry->list = list;
    list->length++;
}

/**
 * @brief Appends an entry to the list.
 *
 * @param list A pointer to the `list_t` to append to.
 * @param prev A pointer to the list entry after which the new entry will be appended.
 * @param entry A pointer to the `list_entry_t` to append.
 */
static inline void list_append(list_t* list, list_entry_t* prev, list_entry_t* entry)
{
    list_add(list, prev, prev->next, entry);
}

/**
 * @brief Prepends an entry to the list.
 *
 * @param list A pointer to the `list_t` to prepend to.
 * @param head A pointer to the list entry before which the new entry will be prepended.
 * @param entry A pointer to the `list_entry_t` to prepend.
 */
static inline void list_prepend(list_t* list, list_entry_t* head, list_entry_t* entry)
{
    list_add(list, head->prev, head, entry);
}

/**
 * @brief Removes a list entry from its current list.
 *
 * @param list A pointer to the `list_t` that contains the entry.
 * @param entry A pointer to the `list_entry_t` to remove.
 */
static inline void list_remove(list_t* list, list_entry_t* entry)
{
    if (entry->list == NULL)
    {
        return;
    }

    assert(list != NULL);
    assert(entry != NULL);
    assert(list->length > 0);
    assert(entry != &list->head);
    assert(entry->list == list);

    entry->prev->next = entry->next;
    entry->next->prev = entry->prev;
    list_entry_init(entry);

    entry->list = NULL;
    list->length--;
}

/**
 * @brief Pushes an entry to the end of the list.
 *
 * @param list A pointer to the `list_t` to push the entry to.
 * @param entry A pointer to the `list_entry_t` to push.
 */
static inline void list_push_back(list_t* list, list_entry_t* entry)
{
    assert(list != NULL);
    assert(entry != NULL);
    assert(entry->next == entry && entry->prev == entry);

    list_add(list, list->head.prev, &list->head, entry);
}

/**
 * @brief Pushes an entry to the front of the list.
 *
 * @param list A pointer to the `list_t` to push the entry to.
 * @param entry A pointer to the `list_entry_t` to push.
 */
static inline void list_push_front(list_t* list, list_entry_t* entry)
{
    assert(list != NULL);
    assert(entry != NULL);
    assert(entry->next == entry && entry->prev == entry);

    list_add(list, &list->head, list->head.next, entry);
}

/**
 * @brief Pops the first entry from the list.
 *
 * @param list A pointer to the `list_t` to pop the entry from.
 * @return A pointer to the removed `list_entry_t`, or `NULL` if the list is empty.
 */
static inline list_entry_t* list_pop_first(list_t* list)
{
    assert(list != NULL);

    if (list_is_empty(list))
    {
        return NULL;
    }

    list_entry_t* entry = list->head.next;
    list_remove(list, entry);
    return entry;
}

/**
 * @brief Pops the last entry from the list.
 *
 * @param list A pointer to the `list_t` to pop the entry from.
 * @return A pointer to the removed `list_entry_t`, or `NULL` if the list is empty.
 */
static inline list_entry_t* list_pop_last(list_t* list)
{
    assert(list != NULL);

    if (list_is_empty(list))
    {
        return NULL;
    }

    list_entry_t* entry = list->head.prev;
    list_remove(list, entry);
    return entry;
}

/**
 * @brief Gets the first entry in the list without removing it.
 *
 * @param list A pointer to the `list_t`.
 * @return A pointer to the first `list_entry_t` in the list, or `NULL` if the list is empty.
 */
static inline list_entry_t* list_first(list_t* list)
{
    assert(list != NULL);

    if (list_is_empty(list))
    {
        return NULL;
    }
    return list->head.next;
}

/**
 * @brief Gets the last entry in the list without removing it.
 *
 * @param list A pointer to the `list_t`.
 * @return A pointer to the last `list_entry_t` in the list, or `NULL` if the list is empty.
 */
static inline list_entry_t* list_last(list_t* list)
{
    assert(list != NULL);

    if (list_is_empty(list))
    {
        return NULL;
    }
    return list->head.prev;
}

/**
 * @brief Gets the next entry in the list relative to a given entry.
 *
 * @param list A pointer to the `list_t`.
 * @param entry A pointer to the `list_entry_t` to get the next entry from.
 * @return A pointer to the next `list_entry_t` in the list, or `NULL` if the given entry is the last.
 */
static inline list_entry_t* list_next(list_t* list, list_entry_t* entry)
{
    assert(list != NULL);
    assert(entry != NULL);
    assert(entry->list == list);

    if (entry->next == &list->head)
    {
        return NULL;
    }
    return entry->next;
}

/**
 * @brief Gets the previous entry in the list relative to a given entry.
 *
 * @param list A pointer to the `list_t`.
 * @param entry A pointer to the `list_entry_t` to get the previous entry from.
 * @return A pointer to the previous `list_entry_t` in the list, or `NULL` if the given entry is the first.
 */
static inline list_entry_t* list_prev(list_t* list, list_entry_t* entry)
{
    assert(list != NULL);
    assert(entry != NULL);
    assert(entry->list == list);

    if (entry->prev == &list->head)
    {
        return NULL;
    }
    return entry->prev;
}

#endif

/** @} */
