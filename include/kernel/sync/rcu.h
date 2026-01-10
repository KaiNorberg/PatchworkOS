#pragma once

#include <kernel/cpu/interrupt.h>

#include <stdint.h>
#include <sys/io.h>
#include <sys/list.h>
#include <sys/proc.h>
#include <stdatomic.h>

/**
 * @brief Read-Copy-Update (RCU) primitive.
 * @defgroup kernel_sync_rcu Read-Copy-Update (RCU) 
 * @ingroup kernel_sync
 *
 * RCU is a synchronization mechanism that allows readers to access shared data structures concurrently with writers,
 * without using locks. It works by deferring the reclamation of old data until all readers that might have been
 * accessing it have completed their operations.
 *
 * @see https://en.wikipedia.org/wiki/Read-copy-update for more information about RCU.
 * @see https://www.kernel.org/doc/Documentation/RCU/whatisRCU.txt for a explanation of RCU in the Linux kernel.
 * 
 * @{
 */

/**
 * @brief RCU callback function type.
 *
 * @param arg The argument passed to the callback.
 */
typedef void (*rcu_callback_t)(void*);

/**
 * @brief Intrusive RCU head structure.
 *
 * Used to queue objects for freeing.
 */
typedef struct rcu_head
{
    struct rcu_head* next;
    rcu_callback_t func;
} rcu_head_t;

/**
 * @brief Per-cpu RCU context.
 * @struct rcu_ctx_t
 */
typedef struct rcu_cpu
{
    rcu_head_t* head;
} rcu_cpu_t;

/**
 * @brief Initializes the per-CPU RCU context.
 *
 * @param rcu The RCU context to initialize.
 */
static inline void rcu_cpu_init(rcu_cpu_t* rcu)
{
    rcu->head = NULL;
}

/**
 * @brief Publishes a pointer to a new data structure.
 *
 * Ensures that the initialization of the structure is visible to readers before the pointer is updated.
 */
#define RCU_ASSIGN_POINTER(p, v) \
    ({ \
        atomic_thread_fence(memory_order_release); \
        (p) = (v); \
    })

/**
 * @brief Fetches a RCU-protected pointer.
 *
 * Ensures that the pointer fetch is ordered before any subsequent data access.
 */
#define RCU_DEREFERENCE(p) \
    ({ \
        atomic_thread_fence(memory_order_consume); \
        (p); \
    })

/**
 * @brief RCU read-side critical section begin.
 *
 * Should be called before accessing RCU protected data.
 */
static inline void rcu_read_lock(void)
{
    interrupt_disable();    
}

/**
 * @brief RCU read-side critical section end.
 *
 * Should be called after accessing RCU protected data.
 */
static inline void rcu_read_unlock(void)
{
    interrupt_enable();
}

/**
 * @brief Wait for all pre-existing RCU read-side critical sections to complete.
 *
 * This function blocks until all RCU read-side critical sections that were active
 * at the time of the call have completed.
 */
void rcu_synchronize(void);

/**
 * @brief Add a callback to be executed after a grace period.
 *
 * @param head The RCU head structure embedded in the object to be freed.
 * @param func The callback function to execute.
 */
void rcu_call(rcu_head_t* head, rcu_callback_t func);

/** @} */