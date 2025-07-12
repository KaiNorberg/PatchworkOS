#pragma once

#include <stdatomic.h>

/**
 * @brief Reference counting.
 * @ingroup kernel
 * @defgroup kernel_ref
 */

#define REF_MAGIC 0x26CB6E4C

/**
 * @brief Reference counting struct.
 * @ingroup kernel_ref
 *
 * The `ref_t` structure must be placed as the first element in a struct.
 */
typedef struct ref
{
#ifndef NDEBUG
    uint32_t magic;
#endif
    atomic_uint32_t count;
    void (*free)(void* self);
} ref_t;

#define REF_DEFER(ptr) __attribute__((cleanup(ref_defer_cleanup))) void* CONCAT(p, __COUNTER__) = (ptr)

void ref_init(ref_t* ref, void* free);

void* ref_inc(void* ptr);

void ref_dec(void* ptr);

static inline void ref_defer_cleanup(void** ptr)
{
    ref_dec(*ptr);
}
