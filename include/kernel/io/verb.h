#pragma once

#include <kernel/io/irp.h>

#include <sys/ioring.h>

/**
 * @brief I/O Request Packet verbs.
 * @defgroup kernel_io_verb I/O Request Packet Verbs
 * @ingroup kernel_io
 *
 * @{
 */

/**
 * @brief Verb function type.
 * 
 * @param irp Pointer to the IRP.
 */
typedef void (*verb_func_t)(irp_t* irp);

/**
 * @brief Verb table structure.
 * @struct verb_table_t
 */
typedef struct verb_table
{
    verb_func_t handlers[VERB_MAX];
} verb_table_t;

/**
 * @brief Cleanup the arguments used by a verb.
 * 
 * Handles both kernel IRPs and parsed user IRPs.
 * 
 * @param irp Pointer to the IRP.
 */
void verb_args_cleanup(irp_t* irp);

/**
 * @brief Dispatch an IRP to the appropriate verb handler.
 * 
 * If the IRP is a user IRP, the arguments will be parsed before invoking the handler.
 * 
 * @param irp Pointer to the IRP.
 */
void verb_dispatch(irp_t* irp);

/**
 * @brief Invoke the appropriate verb handler from a verb table.
 *
 * @param irp Pointer to the IRP.
 * @param table Pointer to the verb table.
 * @return `true` if the IRP was completed, `false` otherwise.
 */
static inline bool verb_invoke(irp_t* irp, const verb_table_t* table)
{
    if (UNLIKELY(irp->verb >= VERB_MAX))
    {
        irp_error(irp, EINVAL);
        return true;
    }

    if (table == NULL)
    {
        return false;
    }

    verb_func_t handler = table->handlers[irp->verb];
    if (handler == NULL)
    {
        return false;
    }

    handler(irp);
    return true;
}

/**
 * @brief Execute an IRP synchronously.
 *
 * This function will dispatch the IRP and blocks the current thread until the operation is complete.
 *
 * @warning This function should only be used when the alternative of using asynchronous operations is simply not worth
 * the complexity. For example, while loading modules.
 *
 * @param irp Pointer to the IRP to execute.
 */
void verb_run(irp_t* irp);

/** @} */