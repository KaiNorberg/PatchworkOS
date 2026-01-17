#pragma once

#include <kernel/sync/lock.h>

#include <assert.h>
#include <stdlib.h>
#include <sys/io.h>
#include <sys/list.h>
#include <sys/math.h>
#include <time.h>

typedef struct process process_t;

/**
 * @brief Asynchronous Request Primitive
 * @defgroup kernel_sync_request Request
 * @ingroup kernel_sync
 *
 * ## Callbacks
 *
 * Requests can define three callbacks, included is a list of their expected semantics.
 *
 * ### Completion Callback
 *
 * The `complete()` callback should be called when the request has been completed, the `complete()` implementation does
 * not need to guarantee that the request structure will remain valid after a call to this function.
 *
 * ### Cancellation Callback
 *
 * The optional `cancel()` callback is called when attempting to cancel an in-progress request, if the request cannot be
 * cancelled, the callback should return `false`, otherwise `true`.
 *
 * ### Timeout Callback
 *
 * The optional `timeout()` callback is called when a request has timed out, the request * will be removed from the
 * timeout queue before this callback is called and it will never * be called more than once.
 *
 * @{
 */

/**
 * @brief Per-CPU request queues.
 * @struct request_ctx_t
 */
typedef struct request_ctx
{
    list_t timeouts;
    lock_t lock;
} request_ctx_t;

/**
 * @brief Task flags.
 * @enum request_flags_t
 */
typedef enum
{
    REQUEST_DELAYED = 1 << 0, ///< The completion of the request has been delayed.
    REQUEST_TIMEOUT = 1 << 1, ///< The request is in a timeout queue.
} request_flags_t;

/**
 * @brief Macro to define common request structure members.
 *
 * All requests contain the below common members:
 * - `list_entry_t entry` - List entry for requests queues and completion queues.
 * - `list_entry_t timeoutEntry` - List entry for timeout queues.
 * - `request_ctx_t* ctx` - Pointer to the per-CPU request context storing this request for timeouts.
 * - `process_t* process` - Pointer to the process that created the request.
 * - `void* data` - Pointer to user data.
 * - `void (*complete)(_type*)` - Completion callback.
 * - `bool (*cancel)(_type*)` - Cancellation callback.
 * - `void (*timeout)(_type*)` - Timeout callback.
 * - `request_flags_t flags` - Task flags.
 * - `errno_t err` - Error code for the request.
 * - `clock_t deadline` - Deadline for the request.
 * - `_resultType result` - Result of the request.
 *
 * @param _type The type of the request structure.
 * @param _resultType The type of the request result.
 */
#define REQUEST_COMMON(_type, _resultType) \
    list_entry_t entry; \
    list_entry_t timeoutEntry; \
    request_ctx_t* ctx; \
    process_t* process; \
    void* data; \
    void (*complete)(_type*); \
    bool (*cancel)(_type*); \
    void (*timeout)(_type*); \
    request_flags_t flags; \
    errno_t err; \
    clock_t deadline; \
    _resultType result

/**
 * @brief Generic request structure.
 * @struct request_t
 *
 * @warning Due to optimization done while allocating requests in the async system, no request structure should be
 * larger than this structure.
 */
typedef struct request
{
    REQUEST_COMMON(struct request, uint64_t);
    uint64_t _padding[4];
} request_t;

/**
 * @brief Task queue structure.
 * @struct request_queue_t
 */
typedef struct
{
    list_t requests;
} request_queue_t;

/**
 * @brief Initializes a request queue.
 *
 * @param queue Pointer to the request queue to initialize.
 */
static inline void request_queue_init(request_queue_t* queue)
{
    list_init(&queue->requests);
}

/**
 * @brief Adds a request to the per-CPU timeout queue.
 *
 * @param request Pointer to the request to add.
 */
void request_timeout_add(request_t* request);

/**
 * @brief Removes a request from the per-CPU timeout queue.
 *
 * @param request Pointer to the request to remove.
 */
void request_timeout_remove(request_t* request);

/**
 * @brief Checks for request timeouts on the current CPU and handles them.
 *
 * @warning Must be called with interrupts disabled.
 */
void request_timeouts_check(void);

/**
 * @brief Macro to initialize a requests common members.
 *
 * @param _request Pointer to the request to initialize.
 */
#define REQUEST_INIT(_request) \
    ({ \
        (_request)->entry = LIST_ENTRY_CREATE((_request)->entry); \
        (_request)->timeoutEntry = LIST_ENTRY_CREATE((_request)->timeoutEntry); \
        (_request)->ctx = NULL; \
        (_request)->process = NULL; \
        (_request)->data = NULL; \
        (_request)->complete = NULL; \
        (_request)->cancel = NULL; \
        (_request)->timeout = NULL; \
        (_request)->flags = 0; \
        (_request)->err = EOK; \
        (_request)->deadline = CLOCKS_NEVER; \
        (_request)->result = (typeof((_request)->result))0; \
    })

/**
 * @brief Macro to call a function with a request and handle early completions.
 *
 * @param _request Pointer to the request.
 * @param _func Function to call with the request.
 * @return The result of the function call.
 */
#define REQUEST_CALL(_request, _func) \
    ({ \
        typeof((_request)->result) result = _func(_request); \
        if ((_request)->err != EOK) \
        { \
            (_request)->flags &= ~REQUEST_DELAYED; \
            (_request)->complete(_request); \
        } \
        else if (!((_request)->flags & REQUEST_DELAYED)) \
        { \
            (_request)->result = result; \
            (_request)->complete(_request); \
        } \
        result; \
    })

/**
 * @brief Macro to delay the completion of a request without adding it to a queue.
 *
 * Primarily intended for use with timeout handling.
 *
 * @param _request Pointer to the request to delay.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
#define REQUEST_DELAY_NO_QUEUE(_request) \
    ({ \
        uint64_t result = 0; \
        (_request)->flags |= REQUEST_DELAYED; \
        if ((_request)->deadline != CLOCKS_NEVER) \
        { \
            if ((_request)->timeout == NULL) \
            { \
                errno = EINVAL; \
                result = ERR; \
            } \
            else \
            { \
                (_request)->flags |= REQUEST_TIMEOUT; \
                request_timeout_add((request_t*)(_request)); \
            } \
        } \
        result; \
    })

/**
 * @brief Macro to delay the completion of a request.
 *
 * @param _request Pointer to the request to delay.
 * @param _queue Pointer to the request queue to add the request to.
 */
#define REQUEST_DELAY(_request, _queue) \
    ({ \
        list_push_back(&(_queue)->requests, &(_request)->entry); \
        uint64_t result = REQUEST_DELAY_NO_QUEUE(_request); \
        if (result == ERR) \
        { \
            list_remove(&(_request)->entry); \
        } \
        result; \
    })

/**
 * @brief Macro to get the next request from a queue.
 *
 * @param _queue Pointer to the request queue.
 * @param _type The type of the request structure.
 * @return Pointer to the next request, or `NULL` if the queue is empty.
 */
#define REQUEST_NEXT(_queue, _type) \
    (list_is_empty(&(_queue)->requests) ? NULL : CONTAINER_OF(list_first(&(_queue)->requests), _type, entry))

/**
 * @brief Macro to complete a request with an error.
 *
 * @param _request Pointer to the request.
 * @param _errno The errno code.
 */
#define REQUEST_ERROR(_request, _errno) \
    ({ \
        if ((_request)->flags & REQUEST_TIMEOUT) \
        { \
            request_timeout_remove((request_t*)(_request)); \
        } \
        list_remove(&(_request)->entry); \
        (_request)->flags &= ~REQUEST_DELAYED; \
        (_request)->err = (_errno); \
        (_request)->complete((_request)); \
    })

/**
 * @brief Macro to complete a request.
 *
 * @param _request Pointer to the request.
 * @param _result The result of the request.
 */
#define REQUEST_COMPLETE(_request, _result) \
    ({ \
        if ((_request)->flags & REQUEST_TIMEOUT) \
        { \
            request_timeout_remove((request_t*)(_request)); \
        } \
        list_remove(&(_request)->entry); \
        (_request)->flags &= ~REQUEST_DELAYED; \
        (_request)->result = (_result); \
        (_request)->complete((_request)); \
    })

/**
 * @brief Macro to cancel a request.
 *
 * @param _request Pointer to the request.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
#define REQUEST_CANCEL(_request) \
    ({ \
        uint64_t result = 0; \
        if ((_request)->cancel == NULL) \
        { \
            errno = EINVAL; \
            result = ERR; \
        } \
        else \
        { \
            (_request)->err = ECANCELED; \
            if (!((_request)->cancel(_request))) \
            { \
                (_request)->err = EOK; \
                errno = EBUSY; \
                result = ERR; \
            } \
        } \
        result; \
    })

/** @} */