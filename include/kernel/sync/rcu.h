#pragma once

#include <kernel/cpu/cli.h>
#include <kernel/cpu/interrupt.h>

#include <kernel/sched/sched.h>
#include <stdatomic.h>
#include <stdint.h>
#include <sys/io.h>
#include <sys/list.h>
#include <sys/proc.h>

typedef struct rcu_entry rcu_entry_t;

/**
 * @brief Read-Copy-Update (RCU) primitive.
 * @defgroup kernel_sync_rcu Read-Copy-Update (RCU)
 * @ingroup kernel_sync
 *
 * RCU is a synchronization mechanism that allows readers to access shared data structures concurrently with writers,
 * without using locks.
 *
 * ## Implementation
 *
 * RCU works by delaying the freeing of a resource until its known to be impossible for any CPU to be using said
 * resource.
 *
 * This is implemented by allowing the resource to persist for a grace period, which is defined as the time taken for
 * all CPUs to pass through a quiescent state. A quiescent state is any point at which the CPU is known to not be
 * accessing RCU protected data.
 *
 * In our case, it is illegal for a context switch to occur while accessing RCU protected data, as preemption is
 * disabled using `rcu_read_lock()`. Therefor, we know that once all CPUs, which were not idle, have performed a context
 * switch, they must have passed through a quiescent state and it is thus safe to free any pending resources.
 *
 * ## Using RCU
 *
 * Using RCU is fairly straightforward, any data structure that is to be protected by RCU must include a `rcu_entry_t`
 * member, when the structure is to be freed after use `rcu_call()` should be called with the address of the
 * `rcu_entry_t` member and a callback function that will free the structure.
 *
 * To access RCU protected data, a read-side critical section must be created using `rcu_read_lock()` and
 * `rcu_read_unlock()`, or the `RCU_READ_SCOPE()` macro.
 *
 * @see [Wikipedia](https://en.wikipedia.org/wiki/Read-copy-update) for more information about RCU.
 * @see [kernel.org](https://www.kernel.org/doc/Documentation/RCU/whatisRCU.txt) for a explanation of RCU in the Linux
 * kernel.
 *
 * @{
 */

/**
 * @brief RCU callback function type.
 *
 * @param rcu The RCU entry being processed.
 */
typedef void (*rcu_callback_t)(void* arg);

/**
 * @brief Intrusive RCU head structure.
 *
 * Used to queue objects for freeing.
 */
typedef struct rcu_entry
{
    list_entry_t entry;
    rcu_callback_t func;
    void* arg;
} rcu_entry_t;

/**
 * @brief RCU read-side critical section begin.
 *
 * Should be called before accessing RCU protected data.
 */
static inline void rcu_read_lock(void)
{
    sched_disable();
}

/**
 * @brief RCU read-side critical section end.
 *
 * Should be called after accessing RCU protected data.
 */
static inline void rcu_read_unlock(void)
{
    sched_enable();
}

/**
 * @brief RCU read-side critical section for the current scope.
 */
#define RCU_READ_SCOPE() \
    rcu_read_lock(); \
    __attribute__((cleanup(rcu_read_unlock_cleanup))) int CONCAT(r, __COUNTER__) = 1;

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
 * @param entry The RCU entry structure embedded in the object to be freed.
 * @param func The callback function to execute.
 */
void rcu_call(rcu_entry_t* entry, rcu_callback_t func, void* arg);

/**
 * @brief Called during a context switch to report a quiescent state.
 */
void rcu_report_quiescent(void);

/**
 * @brief Helper callback to free a pointer.
 *
 * Can be used as a generic callback to free memory allocated with `malloc()`.
 *
 * @param arg The pointer to free.
 */
void rcu_call_free(void* arg);

/**
 * @brief Helper callback to free a cache object.
 *
 * Can be used as a generic callback to free memory allocated from a cache.
 *
 * @param arg The pointer to free.
 */
void rcu_call_cache_free(void* arg);

static inline void rcu_read_unlock_cleanup(int* _)
{
    rcu_read_unlock();
}

/** @} */