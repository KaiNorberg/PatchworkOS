#ifndef _SYS_LISC_H
#define _SYS_LISC_H 1

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/io.h>

typedef struct lisc lisc_t;

/**
 * @brief LISt Configuration (LISC)
 * @defgroup libstd_sys_lisc LISC
 * @ingroup libstd
 *
 * The `sys/lisc.h` header provides definitions for parsing and managing S-expression based configuration files.
 *
 * The LISC format is designed to be a flexible, simple, and human-readable way to store hierarchical data in the form
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
 * lisc       := expression
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

#define LISC_ATOM 0 ///< An atom.
#define LISC_LIST 1 ///< A list.

#define LISC_NONE ((1 << 15) - 1) ///< An invalid index.

#define LISC_MAX_DEPTH 16 ///< The maximum number of nested expressions.

/**
 * @brief A LISC item.
 * @struct lisc_item_t
 */
typedef struct lisc_item
{
    uint16_t next : 15; ///< The index of the next LISC item.
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
} lisc_item_t;

#define LISC_SMALL_MAX 64 ///< The size used for small buffer optimization in the `lisc_t` structure.

#define LISC_ITEM_ROOT 0 ///< The index of the root item in the `lisc_t` structure.

/**
 * @brief A reference to a LISC item.
 * @struct lisc_ref_t
 */
typedef struct lisc_ref
{
    lisc_t* lisc;
    uint16_t index;
} lisc_ref_t;

/**
 * @brief LISC structure.
 * @struct lisc_t
 */
typedef struct lisc
{
    const char* input;                 ///< The input buffer.
    lisc_item_t small[LISC_SMALL_MAX]; ///< Small buffer optimization.
    lisc_item_t* items;                ///< Pointer to the array of allocated items.
    uint16_t count;                    ///< The total number of allocated items.
    uint16_t capacity;                 ///< The current capacity of the items array.
    char error[MAX_PATH]; ///< If an error occurs, a detailed human-readable error string will be stored here.
} lisc_t;

/**
 * @brief Parse a LISC buffer into a LISC structure.
 *
 * @param lisc Pointer to the LISC structure to fill.
 * @param input The buffer to parse.
 * @param size The size of the input buffer.
 * @return An appropriate status value.
 */
status_t lisc_init(lisc_t* lisc, const char* input, size_t size);

/**
 * @brief Deinitialize the LISC structure.
 *
 * @param lisc Pointer to the LISC structure to deinitialize.
 */
void lisc_deinit(lisc_t* lisc);

/**
 * @brief Get the root item of the LISC structure.
 *
 * @param lisc Pointer to the LISC structure.
 * @return A reference to the root item.
 */
static inline lisc_ref_t lisc_root(lisc_t* lisc)
{
    return (lisc_ref_t){.lisc = lisc, .index = LISC_ITEM_ROOT};
}

/**
 * @brief Get the next item in the list.
 *
 * @param entry The reference to an item in a list.
 * @return A reference to the next item in the list, or a reference with index `LISC_NONE` if there is no next item.
 */
static inline lisc_ref_t lisc_next(lisc_ref_t entry)
{
    if (entry.index == LISC_NONE)
    {
        return entry;
    }

    return (lisc_ref_t){.lisc = entry.lisc, .index = entry.lisc->items[entry.index].next};
}

/**
 * @brief Get the first item of a list.
 *
 * @param ref The reference to the list item.
 * @return A reference to the first item in the list, or a reference with index `LISC_NONE` if the list is empty or the
 * item is an atom.
 */
static inline lisc_ref_t lisc_first(lisc_ref_t list)
{
    if (list.index == LISC_NONE || list.lisc->items[list.index].type == LISC_ATOM)
    {
        return (lisc_ref_t){.lisc = list.lisc, .index = LISC_NONE};
    }

    return (lisc_ref_t){.lisc = list.lisc, .index = list.lisc->items[list.index].list.first};
}

/**
 * @brief Get the last item of a list.
 *
 * @param list The reference to the list item.
 * @return A reference to the last item in the list, or a reference with index `LISC_NONE` if the list is empty or the
 * item is an atom.
 */
static inline lisc_ref_t lisc_last(lisc_ref_t list)
{
    if (list.index == LISC_NONE || list.lisc->items[list.index].type == LISC_ATOM)
    {
        return (lisc_ref_t){.lisc = list.lisc, .index = LISC_NONE};
    }

    return (lisc_ref_t){.lisc = list.lisc, .index = list.lisc->items[list.index].list.last};
}

/**
 * @brief Check if a reference is an atom.
 *
 * @param ref The reference to check.
 * @return `true` if the reference is an atom, `false` otherwise.
 */
static inline bool lisc_is_atom(lisc_ref_t ref)
{
    return ref.index != LISC_NONE && ref.lisc->items[ref.index].type == LISC_ATOM;
}

/**
 * @brief Check if a reference is a list.
 *
 * @param ref The reference to check.
 * @return `true` if the reference is a list, `false` otherwise.
 */
static inline bool lisc_is_list(lisc_ref_t ref)
{
    return ref.index != LISC_NONE && ref.lisc->items[ref.index].type == LISC_LIST;
}

/**
 * @brief Check if a reference is valid.
 *
 * @param ref The reference to check.
 * @return `true` if the reference is valid, `false` otherwise.
 */
static inline bool lisc_is_valid(lisc_ref_t ref)
{
    return ref.lisc != NULL && ref.index != LISC_NONE;
}

/**
 * @brief Check if two references are equal.
 *
 * @param a The first reference.
 * @param b The second reference.
 * @return `true` if the references are equal, `false` otherwise.
 */
static inline bool lisc_compare(lisc_ref_t a, lisc_ref_t b)
{
    return a.lisc == b.lisc && a.index == b.index;
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
static inline status_t lisc_atom_get(lisc_ref_t ref, const char** out, size_t* len)
{
    if (!lisc_is_valid(ref) || !lisc_is_atom(ref))
    {
        return ERR(LIBSTD, INVAL);
    }

    lisc_item_t* item = &ref.lisc->items[ref.index];
    *out = &ref.lisc->input[item->atom.start];
    *len = item->atom.end - item->atom.start;
    return OK;
}

/**
 * @brief Get the length of an atom.
 *
 * @param ref The reference to the atom.
 * @return The length of the atom in bytes.
 */
static inline size_t lisc_atom_len(lisc_ref_t ref)
{
    if (!lisc_is_valid(ref) || !lisc_is_atom(ref))
    {
        return 0;
    }

    lisc_item_t* item = &ref.lisc->items[ref.index];
    return item->atom.end - item->atom.start;
}

/**
 * @brief Get a pointer to the start of an atom's string value.
 *
 * @note The string is not null-terminated. Use `lisc_atom_len()` to get the length.
 *
 * @param ref The reference to the atom.
 * @return A pointer to the start of the atom's string value, or `NULL` if the reference is invalid or not an atom.
 */
static inline const char* lisc_atom_str(lisc_ref_t ref)
{
    if (!lisc_is_valid(ref) || !lisc_is_atom(ref))
    {
        return NULL;
    }

    lisc_item_t* item = &ref.lisc->items[ref.index];
    return &ref.lisc->input[item->atom.start];
}

/**
 * @brief Compare an atom's value with a string.
 *
 * @param ref The reference to the atom.
 * @param str The string to compare against.
 * @return `true` if the atom matches the string, `false` otherwise.
 */
static inline bool lisc_atom_eq(lisc_ref_t ref, const char* str)
{
    if (!lisc_is_valid(ref) || !lisc_is_atom(ref))
    {
        return false;
    }

    lisc_item_t* item = &ref.lisc->items[ref.index];
    size_t len = item->atom.end - item->atom.start;
    if (len != strlen(str))
    {
        return false;
    }
    return strncmp(&ref.lisc->input[item->atom.start], str, len) == 0;
}

/**
 * @brief Find a sub-list by its head atom name.
 *
 * Given a list, this searches for a child item that is itself a list whose first element
 * is an atom matching `name`.
 *
 * @param list The reference to the parent list.
 * @param name The name of the head atom to search for.
 * @return A reference to the found list, or a reference with index `LISC_NONE` if not found.
 */
static inline lisc_ref_t lisc_find(lisc_ref_t list, const char* name)
{
    if (!lisc_is_valid(list) || !lisc_is_list(list))
    {
        return (lisc_ref_t){.lisc = list.lisc, .index = LISC_NONE};
    }

    lisc_ref_t first = lisc_first(list);
    while (lisc_is_valid(first))
    {
        if (lisc_is_list(first))
        {
            lisc_ref_t head = lisc_first(first);
            if (lisc_is_atom(head) && lisc_atom_eq(head, name))
            {
                return first;
            }
        }
        first = lisc_next(first);
    }

    return (lisc_ref_t){.lisc = list.lisc, .index = LISC_NONE};
}

/**
 * @brief Macro for iterating over the items of a LISC list.
 *
 * @param _item The loop variable, a `lisc_ref_t`.
 * @param _list The reference to the list to iterate over.
 */
#define LISC_FOR_EACH(_item, _list) \
    for (lisc_ref_t _item = lisc_first(_list); lisc_is_valid(_item); (_item) = lisc_next(_item))

/** @} */

#endif