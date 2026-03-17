#pragma once

#include <kernel/log/log.h>
#include <kernel/sync/lock.h>

#include <sys/defs.h>
#include <sys/list.h>

#include <assert.h>
#include <stdatomic.h>
#include <stdint.h>

typedef struct ref ref_t;

/**
 * @brief Reference counting.
 * @defgroup kernel_utils_ref Reference counting
 * @ingroup kernel_utils
 *
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
    uint32_t magic;
#endif
    atomic_uint32_t count;
    void (*callback)(void* self); ///< Cleanup function called when count reaches zero
} ref_t;

/**
 * @brief Get current reference count.
 *
 * Primarily intended to be used with RCU protected objects to check if they are still alive within a RCU read critical
 * section.
 *
 * @param ptr Pointer to the struct containing `ref_t` as its first member, can be `NULL`.
 * @return Current reference count, or `0` if `ptr` is `NULL`.
 */
#define REF_COUNT(ptr) ((ptr) == NULL ? 0 : atomic_load_explicit(&((ref_t*)(ptr))->count, memory_order_relaxed))

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
        ref_t* ref = (ref_t*)(ptr); \
        ref_inc(ref); \
        ptr; \
    })

/**
 * @brief Increment reference count, but only if the current count is not zero.
 *
 * @param ptr Pointer to the struct containing `ref_t` as its first member, can be `NULL`.
 * @return The `ptr` passed as input, or `NULL` if the count was zero.
 */
#define REF_TRY(ptr) \
    ({ \
        ref_t* ref = (ref_t*)(ptr); \
        ref_inc_try(ref) != NULL ? ptr : NULL; \
    })

/**
 * @brief Decrement reference count
 *
 * Atomically decrements the reference counter. Used to avoid the need for a typecast. The magic number checking makes
 * sure we cant accidentally misuse this.
 *
 * @param ptr Pointer to the struct containing `ref_t` as its first member, can be `NULL`.
 */
#define UNREF(ptr) \
    ({ \
        ref_t* ref = (ref_t*)(ptr); \
        ref_dec(ref); \
    })

/**
 * @brief RAII-style cleanup for scoped references
 *
 * Uses GCC's cleanup attribute to automatically call `ref_dec` when going out of scope.
 *
 * @param ptr Pointer to the struct containing `ref_t` as its first member, can be `NULL`.
 */
#define UNREF_DEFER(ptr) __attribute__((cleanup(ref_defer_cleanup))) void* CONCAT(p, __COUNTER__) = (ptr)

/**
 * @brief Initialize a reference counter
 *
 * @param ref Pointer to the reference counter structure
 * @param callback Callback to call when count reaches zero
 */
static inline void ref_init(ref_t* ref, void* callback)
{
#ifndef NDEBUG
    ref->magic = REF_MAGIC;
#endif
    atomic_init(&ref->count, 1);
    ref->callback = callback;
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
 * @brief Increment reference count, but only if the current count is not zero.
 *
 * @param ptr Pointer to the struct containing `ref_t` as its first member, can be `NULL`.
 * @return The `ptr` passed as input, or `NULL` if the count was zero.
 */
static inline void* ref_inc_try(void* ptr)
{
    ref_t* ref = (ref_t*)ptr;
    if (ref == NULL)
    {
        return NULL;
    }

    assert(ref->magic == REF_MAGIC);
    uint32_t count = atomic_load_explicit(&ref->count, memory_order_relaxed);
    do
    {
        if (count == 0)
        {
            return NULL;
        }
    } while (!atomic_compare_exchange_weak_explicit(&ref->count, &count, count + 1, memory_order_relaxed,
        memory_order_relaxed));
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
    uint64_t count = atomic_fetch_sub_explicit(&ref->count, 1, memory_order_release);
    if (count > 1)
    {
        return;
    }

    atomic_thread_fence(memory_order_acquire);
    assert(count == 1); // Count is now zero, if it was zero before then we have a double free.

    if (ref->callback == NULL)
    {
        return;
    }

    ref->callback(ptr);
}

static inline void ref_defer_cleanup(void** ptr)
{
    ref_dec(*ptr);
}

/** @} */
