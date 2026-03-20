#ifndef _SYS_SCON_H
#define _SYS_SCON_H 1

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/io.h>

typedef struct scon scon_t;

/**
 * @brief S-expression CONfig (SCON)
 * @defgroup libstd_sys_scon SCON
 * @ingroup libstd
 *
 * The `sys/scon.h` header provides definitions for parsing and managing S-expression based configuration files.
 *
 * The SCON format is designed to be a flexible, simple, and human-readable way to store hierarchical data in the form
 * of recursive lists of strings while also allowing for surprisingly fast parsing.
 *
 * It is primarily used for component manifests and system configuration.
 *
 * ## Example
 *
 * ```lisp
 * (component
 *     (description "An example component.")
 *     (author Kai)
 *     (license MIT)
 *     (launch bin/example)
 *     (dependencies
 *         (libstd >= 1.0.0)
 *     )
 *     (capabilities
 *         fb
 *         kbd
 *     )
 * )
 * ```
 *
 * ### Grammar
 *
 * ```
 * scon       := expression
 * expression := '(' list ')'
 * list       := ε | item list
 * item       := atom | expression
 * atom       := unquoted | quoted
 * unquoted   := [^ \f\n\r\t\v()"]+
 * quoted     := '"' [^"]* '"'
 * ```
 *
 * Note that whitespace, defined by the `isspace()` macro, is used to separate atoms and expressions, but is otherwise
 * trimmed.
 *
 * The first atom in a list is as convention used to specify the meaning of the expression, however the format itself
 * does not enforce this.
 *
 * @{
 */

#define SCON_ATOM 0 ///< An atom.
#define SCON_LIST 1 ///< A list.

#define SCON_NONE ((1 << 15) - 1) ///< An invalid index.

#define SCON_MAX_DEPTH 16 ///< The maximum number of nested expressions.

/**
 * @brief A SCON item.
 * @struct scon_item_t
 */
typedef struct scon_item
{
    uint16_t next : 15; ///< The index of the next SCON item.
    uint16_t type : 1;  ///< The type of the item.
    union {
        struct
        {
            uint16_t start; ///< The index of the start of the atom within the input buffer.
            uint16_t end;   ///< The index of the end of the atom within the input buffer.
        } atom;
        struct
        {
            uint16_t first; ///< The index of the first object in the list.
            uint16_t last;  ///< The index of the last object in the list.
        } list;
    };
} scon_item_t;

#define SCON_SMALL_MAX 64 ///< The size used for small buffer optimization in the `scon_t` structure.

#define SCON_ITEM_ROOT 0 ///< The index of the root item in the `scon_t` structure.

/**
 * @brief A reference to a SCON item.
 * @struct scon_ref_t
 */
typedef struct scon_ref
{
    scon_t* scon;
    uint16_t index;
} scon_ref_t;

/**
 * @brief SCON structure.
 * @struct scon_t
 */
typedef struct scon
{
    const char* input;                 ///< The input buffer.
    scon_item_t small[SCON_SMALL_MAX]; ///< Small buffer optimization.
    scon_item_t* items;                ///< Pointer to the array of allocated items.
    uint16_t count;                    ///< The total number of allocated items.
    uint16_t capacity;                 ///< The current capacity of the items array.
    char error[MAX_PATH]; ///< If an error occurs, a detailed human-readable error string will be stored here.
} scon_t;

/**
 * @brief Parse a SCON buffer into a SCON structure.
 *
 * @param scon Pointer to the SCON structure to fill.
 * @param input The buffer to parse.
 * @param size The size of the input buffer.
 * @return An appropriate status value.
 */
status_t scon_init(scon_t* scon, const char* input, size_t size);

/**
 * @brief Deinitialize the SCON structure.
 *
 * @note Will not free the input buffer.
 * 
 * @param scon Pointer to the SCON structure to deinitialize.
 */
void scon_deinit(scon_t* scon);

/**
 * @brief Get the root item of the SCON structure.
 *
 * @param scon Pointer to the SCON structure.
 * @return A reference to the root item.
 */
static inline scon_ref_t scon_root(scon_t* scon)
{
    return (scon_ref_t){.scon = scon, .index = SCON_ITEM_ROOT};
}

/**
 * @brief Get the next item in the list.
 *
 * @param entry The reference to an item in a list.
 * @return A reference to the next item in the list, or a reference with index `SCON_NONE` if there is no next item.
 */
static inline scon_ref_t scon_next(scon_ref_t entry)
{
    if (entry.index == SCON_NONE)
    {
        return entry;
    }

    return (scon_ref_t){.scon = entry.scon, .index = entry.scon->items[entry.index].next};
}

/**
 * @brief Get the first item of a list.
 *
 * @param ref The reference to the list item.
 * @return A reference to the first item in the list, or a reference with index `SCON_NONE` if the list is empty or the
 * item is an atom.
 */
static inline scon_ref_t scon_first(scon_ref_t list)
{
    if (list.index == SCON_NONE || list.scon->items[list.index].type == SCON_ATOM)
    {
        return (scon_ref_t){.scon = list.scon, .index = SCON_NONE};
    }

    return (scon_ref_t){.scon = list.scon, .index = list.scon->items[list.index].list.first};
}

/**
 * @brief Get the last item of a list.
 *
 * @param list The reference to the list item.
 * @return A reference to the last item in the list, or a reference with index `SCON_NONE` if the list is empty or the
 * item is an atom.
 */
static inline scon_ref_t scon_last(scon_ref_t list)
{
    if (list.index == SCON_NONE || list.scon->items[list.index].type == SCON_ATOM)
    {
        return (scon_ref_t){.scon = list.scon, .index = SCON_NONE};
    }

    return (scon_ref_t){.scon = list.scon, .index = list.scon->items[list.index].list.last};
}

/**
 * @brief Check if a reference is an atom.
 *
 * @param ref The reference to check.
 * @return `true` if the reference is an atom, `false` otherwise.
 */
static inline bool scon_is_atom(scon_ref_t ref)
{
    return ref.index != SCON_NONE && ref.scon->items[ref.index].type == SCON_ATOM;
}

/**
 * @brief Check if a reference is a list.
 *
 * @param ref The reference to check.
 * @return `true` if the reference is a list, `false` otherwise.
 */
static inline bool scon_is_list(scon_ref_t ref)
{
    return ref.index != SCON_NONE && ref.scon->items[ref.index].type == SCON_LIST;
}

/**
 * @brief Check if a reference is valid.
 *
 * @param ref The reference to check.
 * @return `true` if the reference is valid, `false` otherwise.
 */
static inline bool scon_is_valid(scon_ref_t ref)
{
    return ref.scon != NULL && ref.index != SCON_NONE;
}

/**
 * @brief Check if two references are equal.
 *
 * @param a The first reference.
 * @param b The second reference.
 * @return `true` if the references are equal, `false` otherwise.
 */
static inline bool scon_compare(scon_ref_t a, scon_ref_t b)
{
    return a.scon == b.scon && a.index == b.index;
}

/**
 * @brief Get the string value of an atom.
 *
 * @note The string is not null-terminated. Use the provided length.
 *
 * @param ref The reference to the atom.
 * @param out Output pointer for the string.
 * @param len Output pointer for the length of the string.
 * @return An appropriate status value.
 */
static inline status_t scon_atom_get(scon_ref_t ref, const char** out, size_t* len)
{
    if (!scon_is_valid(ref) || !scon_is_atom(ref))
    {
        return ERR(LIBSTD, INVAL);
    }

    scon_item_t* item = &ref.scon->items[ref.index];
    *out = &ref.scon->input[item->atom.start];
    *len = item->atom.end - item->atom.start;
    return OK;
}

/**
 * @brief Get the length of an atom.
 *
 * @param ref The reference to the atom.
 * @return The length of the atom in bytes.
 */
static inline size_t scon_atom_len(scon_ref_t ref)
{
    if (!scon_is_valid(ref) || !scon_is_atom(ref))
    {
        return 0;
    }

    scon_item_t* item = &ref.scon->items[ref.index];
    return item->atom.end - item->atom.start;
}

/**
 * @brief Get a pointer to the start of an atom's string value.
 *
 * @note The string is not null-terminated. Use `scon_atom_len()` to get the length.
 *
 * @param ref The reference to the atom.
 * @return A pointer to the start of the atom's string value, or `NULL` if the reference is invalid or not an atom.
 */
static inline const char* scon_atom_str(scon_ref_t ref)
{
    if (!scon_is_valid(ref) || !scon_is_atom(ref))
    {
        return NULL;
    }

    scon_item_t* item = &ref.scon->items[ref.index];
    return &ref.scon->input[item->atom.start];
}

/**
 * @brief Compare an atom's value with a string.
 *
 * @param ref The reference to the atom.
 * @param str The string to compare against.
 * @return `true` if the atom matches the string, `false` otherwise.
 */
static inline bool scon_atom_eq(scon_ref_t ref, const char* str)
{
    if (!scon_is_valid(ref) || !scon_is_atom(ref))
    {
        return false;
    }

    scon_item_t* item = &ref.scon->items[ref.index];
    size_t len = item->atom.end - item->atom.start;
    if (len != strlen(str))
    {
        return false;
    }
    return strncmp(&ref.scon->input[item->atom.start], str, len) == 0;
}

/**
 * @brief Find a sub-list by its head atom name.
 *
 * Given a list, this searches for a child item that is itself a list whose first element
 * is an atom matching `name`.
 *
 * @param list The reference to the parent list.
 * @param name The name of the head atom to search for.
 * @return A reference to the found list, or a reference with index `SCON_NONE` if not found.
 */
static inline scon_ref_t scon_find(scon_ref_t list, const char* name)
{
    if (!scon_is_valid(list) || !scon_is_list(list))
    {
        return (scon_ref_t){.scon = list.scon, .index = SCON_NONE};
    }

    scon_ref_t first = scon_first(list);
    while (scon_is_valid(first))
    {
        if (scon_is_list(first))
        {
            scon_ref_t head = scon_first(first);
            if (scon_is_atom(head) && scon_atom_eq(head, name))
            {
                return first;
            }
        }
        first = scon_next(first);
    }

    return (scon_ref_t){.scon = list.scon, .index = SCON_NONE};
}

/**
 * @brief Retrieve the n-th item in a SCON list.
 * 
 * @param list The reference to the list item.
 * @param n The index of the item to retrieve.
 * @return A reference to the n-th item, or a reference with index `SCON_NONE` if not found.
 */
static inline scon_ref_t scon_get(scon_ref_t list, size_t n)
{
    if (!scon_is_valid(list) || !scon_is_list(list))
    {
        return (scon_ref_t){.scon = list.scon, .index = SCON_NONE};
    }

    scon_ref_t item = scon_first(list);
    for (size_t i = 0; i < n && scon_is_valid(item); i++)
    {
        item = scon_next(item);
    }

    return item;
}

/**
 * @brief Macro for iterating over the items of a SCON list.
 *
 * @param _item The loop variable, a `scon_ref_t`.
 * @param _list The reference to the lis to iterate over.
 * @param _start The index to start iterating from.
 */
#define SCON_FOR_EACH(_item, _list, _start) \
    for (_item = scon_get(_list, _start); scon_is_valid(_item); (_item) = scon_next(_item))

/** @} */

#endif