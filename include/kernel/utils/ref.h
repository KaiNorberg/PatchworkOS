#pragma once

#include <kernel/defs.h>

#include <assert.h>
#include <stdatomic.h>
#include <stdint.h>

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
 *
 * @param ptr Pointer to the struct containing `ref_t` as its first member, can be `NULL`.
 */
#define DEREF_DEFER(ptr) __attribute__((cleanup(ref_defer_cleanup))) void* CONCAT(p, __COUNTER__) = (ptr)

/**
 * @brief Increment reference count
 *
 * Atomically increments the reference counter. Used to avoid the need for a typecast. The magic number checking makes
 * sure we cant accidentally misuse this.
 *
 * @param ptr Pointer to the struct containing `ref_t` as its first member, can be `NULL`.
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
 * Atomically decrements the reference counter. Used to avoid the need for a typecast. The magic number checking makes
 * sure we cant accidentally misuse this.
 *
 * @param ptr Pointer to the struct containing `ref_t` as its first member, can be `NULL`.
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
static inline void ref_init(ref_t* ref, void* free)
{
#ifndef NDEBUG
    ref->magic = REF_MAGIC;
#endif
    atomic_init(&ref->count, 1);
    ref->free = free;
}

/**
 * @brief Increment reference count
 *
 * @param ptr Pointer to the struct containing `ref_t` as its first member, can be `NULL`.
 * @return The `ptr` passed as input
 */
static inline void* ref_inc(void* ptr)
{
    ref_t* ref = (ref_t*)ptr;
    if (ref == NULL)
    {
        return NULL;
    }

    assert(ref->magic == REF_MAGIC);
    atomic_fetch_add_explicit(&ref->count, 1, memory_order_relaxed);
    return ptr;
}

/**
 * @brief Decrement reference count
 *
 * If count reaches zero it calls the registered cleanup function.
 *
 * @param ptr Pointer to the struct containing `ref_t` as its first member, can be `NULL`.
 */
static inline void ref_dec(void* ptr)
{
    ref_t* ref = (ref_t*)ptr;
    if (ref == NULL)
    {
        return;
    }

    assert(ref->magic == REF_MAGIC);
    uint64_t count = atomic_fetch_sub_explicit(&ref->count, 1, memory_order_relaxed);
    if (count > 1)
    {
        return;
    }

    atomic_thread_fence(memory_order_acquire);
    assert(count == 1); // Count is now zero, if it was zero before then we have a double free.
    if (ref->free == NULL)
    {
        return;
    }

#ifndef NDEBUG
    ref->magic = 0;
#endif
    ref->free(ptr);
}

static inline void ref_defer_cleanup(void** ptr)
{
    ref_dec(*ptr);
}

/** @} */
