#ifndef _AUX_CONTAINER_OF_H
#define _AUX_CONTAINER_OF_H 1

/**
 * @brief Container of macro
 * @ingroup libstd
 *
 * The `CONTAINER_OF()` macro can be used to retrieve the parent structure given a pointer to a member of that
 * structure.
 *
 * @param ptr The pointer to the structures member.
 * @param type The name of the perent structures type.
 * @param member The name of the member that `ptr` points to.
 * @return A pointer to the parent structure.
 */
#define CONTAINER_OF(ptr, type, member) ((type*)((char*)(ptr) - offsetof(type, member)))

/**
 * @brief Safe container of macro.
 * @ingroup libstd
 *
 * The `CONTAINER_OF_SAFE()` macro is the same as the `CONTAINER_OF()`, expect that it also handles `NULL` values.
 *
 * @param ptr The pointer to the structures member.
 * @param type The name of the perent structures type.
 * @param member The name of the member that `ptr` points to.
 * @return If `ptr` is not equal to `NULL`, returns a pointer to the parent structure, else returns `NULL`.
 */
#define CONTAINER_OF_SAFE(ptr, type, member) \
    ({ \
        void* p = ptr; \
        ((p != NULL) ? CONTAINER_OF(p, type, member) : NULL); \
    })

#endif