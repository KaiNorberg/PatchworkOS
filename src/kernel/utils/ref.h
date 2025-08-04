#pragma once

#include <common/defs.h>

#include <stdatomic.h>

/**
 * @brief Reference counting
 * @defgroup kernel_utils_ref Reference counting
 * @ingroup kernel_utils
 * @{
 */

/**
 * @brief Magic value used in debug builds to check for corruption or invalid use of the `ref_t` structure.
 */
#define REF_MAGIC 0x26CB6E4C

/**
 * @brief Reference counting structure
 * @struct ref_t
 *
 * Provides a generic interface for reference counting. Must be placed as the first element in any struct that requires
 * reference counting.
 *
 */
typedef struct ref
{
#ifndef NDEBUG
    /**
     * @brief Debug magic value to detect corruption
     */
    uint32_t magic;
#endif
    /**
     * @brief Atomic reference counter
     */
    atomic_uint32_t count;
    /**
     * @brief Cleanup function called when count reaches zero
     */
    void (*free)(void* self);
} ref_t;

/**
 * @brief RAII-style cleanup for scoped references
 *
 * Uses GCC's cleanup attribute to automatically call `ref_dec` when going out of scope.
 */
#define REF_DEFER(ptr) __attribute__((cleanup(ref_defer_cleanup))) void* CONCAT(p, __COUNTER__) = (ptr)

/**
 * @brief Increment reference count
 *
 * Atomically increments the reference counter. Used to avoid the need for a typecast. The magic number checking makes sure we cant accidentally misuse this.
 *
 * @param ptr Pointer to the struct containing `ref_t` as its first member
 * @return The `ptr` passed as input
 */
#define REF(ptr) \
    ({ \
        ref_t* ref = (ref_t*)ptr; \
        ref_inc(ref); \
        ptr; \
    })

/**
 * @brief Decrement reference count
 *
 * Atomically decrements the reference counter. Used to avoid the need for a typecast. The magic number checking makes sure we cant accidentally misuse this.
 *
 * @param ptr Pointer to the struct containing `ref_t` as its first member
 */
#define DEREF(ptr) \
    ({ \
        ref_t* ref = (ref_t*)ptr; \
        ref_dec(ref); \
    })

/**
 * @brief Initialize a reference counter
 *
 * @param ref Pointer to the reference counter structure
 * @param free Cleanup function to call when count reaches zero
 */
void ref_init(ref_t* ref, void* free);

/**
 * @brief Increment reference count
 *
 * @param ptr Pointer to the struct containing `ref_t` as its first member
 * @return The `ptr` passed as input
 */
void* ref_inc(void* ptr);

/**
 * @brief Decrement reference count
 *
 * If count reaches zero it calls the registered cleanup function.
 *
 * @param ptr Pointer to the struct containing `ref_t` as its first member
 */
void ref_dec(void* ptr);

static inline void ref_defer_cleanup(void** ptr)
{
    ref_dec(*ptr);
}

/** @} */
