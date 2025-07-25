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
 * The `sys/list.h` header implements a doubly linked list inspired by the linked list implementation used in the Linux
 * kernel, where the linked list entry structure is stored within each entry instead of each entry having a pointer to
 * each stucture, the structure itself can then be accessed by using `CONTAINER_OF`.
 *
 * It might seem a bit unusual to include something as significant as a doubly linked list in the C standard library,
 * but it is something used to often that having to implemented it for every single program in the OS is massively
 * redundant.
 *
 */

/**
 * @brief A entry in a doubly linked list.
 * @ingroup libstd_sys_list
 * @struct list_entry_t
 *
 * This structure should be placed within another structure so that the `CONTAINER_OF()` macro can then be used to
 * access the other structure.
 */
typedef struct list_entry
{
    struct list_entry* prev; //!< The previous entry in the list
    struct list_entry* next; //!< The next entry in the list
} list_entry_t;

/**
 * @brief A doubly linked list.
 * @ingroup libstd_sys_list
 *
 * This structure simplifies reasoning around linked lists.
 */
typedef struct
{
    list_entry_t head; //!< The head of the list, where head::prev is the last entry of the list and head::next is the
                       //!< first entry of the list.
} list_t;

/**
 * @brief Iterates over a list.
 * @ingroup libstd_sys_list
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
 * @ingroup libstd_sys_list
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
 * @ingroup libstd_sys_list
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
 * @ingroup libstd_sys_list
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
 * @ingroup libstd_sys_list
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
 * @ingroup libstd_sys_list
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
 * @ingroup libstd_sys_list
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
 * @brief Initializes a list entry.
 * @ingroup libstd_sys_list
 *
 * @param entry A pointer to the `list_entry_t` to initialize.
 */
static inline void list_entry_init(list_entry_t* entry)
{
    entry->next = entry;
    entry->prev = entry;
}

/**
 * @brief Initializes a list.
 * @ingroup libstd_sys_list
 *
 * @param list A pointer to the `list_t` to initialize.
 */
static inline void list_init(list_t* list)
{
    list_entry_init(&list->head);
}

/**
 * @brief Checks if a list is empty.
 * @ingroup libstd_sys_list
 *
 * @param list A pointer to the `list_t` to check.
 * @return `true` if the list is empty, `false` otherwise.
 */
static inline bool list_is_empty(list_t* list)
{
    return list->head.next == &list->head;
}

/**
 * @brief Adds a new element between two existing list entries.
 * @ingroup libstd_sys_list
 *
 * @param prev A pointer to the list entry that will precede the new element.
 * @param next A pointer to the list entry that will follow the new element.
 * @param elem A pointer to the `list_entry_t` to add.
 */
static inline void list_add(list_entry_t* prev, list_entry_t* next, list_entry_t* elem)
{
    next->prev = elem;
    elem->next = next;
    elem->prev = prev;
    prev->next = elem;
}

/**
 * @brief Appends an entry to the list.
 * @ingroup libstd_sys_list
 *
 * @param prev A pointer to the list entry after which the new entry will be appended.
 * @param entry A pointer to the `list_entry_t` to append.
 */
static inline void list_append(list_entry_t* prev, list_entry_t* entry)
{
    list_add(prev, prev->next, entry);
}

/**
 * @brief Prepends an entry to the list.
 * @ingroup libstd_sys_list
 *
 * @param head A pointer to the list entry before which the new entry will be prepended.
 * @param entry A pointer to the `list_entry_t` to prepend.
 */
static inline void list_prepend(list_entry_t* head, list_entry_t* entry)
{
    list_add(head->prev, head, entry);
}

/**
 * @brief Removes a list entry from its current list.
 * @ingroup libstd_sys_list
 *
 * @param entry A pointer to the `list_entry_t` to remove.
 */
static inline void list_remove(list_entry_t* entry)
{
    entry->prev->next = entry->next;
    entry->next->prev = entry->prev;
    list_entry_init(entry);
}

/**
 * @brief Pushes an entry to the end of the list.
 * @ingroup libstd_sys_list
 *
 * @param list A pointer to the `list_t` to push the entry to.
 * @param entry A pointer to the `list_entry_t` to push.
 */
static inline void list_push(list_t* list, list_entry_t* entry)
{
    assert(entry->next == entry && entry->prev == entry);
    list_add(list->head.prev, &list->head, entry);
}

/**
 * @brief Pops the first entry from the list.
 * @ingroup libstd_sys_list
 *
 * @param list A pointer to the `list_t` to pop the entry from.
 * @return A pointer to the removed `list_entry_t`, or `NULL` if the list is empty.
 */
static inline list_entry_t* list_pop(list_t* list)
{
    if (list_is_empty(list))
    {
        return NULL;
    }

    list_entry_t* entry = list->head.next;
    list_remove(entry);
    return entry;
}

/**
 * @brief Gets the first entry in the list without removing it.
 * @ingroup libstd_sys_list
 *
 * @param list A pointer to the `list_t`.
 * @return A pointer to the first `list_entry_t` in the list, or `NULL` if the list is empty.
 */
static inline list_entry_t* list_first(list_t* list)
{
    if (list_is_empty(list))
    {
        return NULL;
    }
    return list->head.next;
}

/**
 * @brief Gets the last entry in the list without removing it.
 * @ingroup libstd_sys_list
 *
 * @param list A pointer to the `list_t`.
 * @return A pointer to the last `list_entry_t` in the list, or `NULL` if the list is empty.
 */
static inline list_entry_t* list_last(list_t* list)
{
    if (list_is_empty(list))
    {
        return NULL;
    }
    return list->head.prev;
}

#endif
