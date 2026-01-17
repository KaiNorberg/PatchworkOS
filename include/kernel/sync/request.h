#pragma once

#include <kernel/sync/lock.h>

#include <assert.h>
#include <stdlib.h>
#include <sys/io.h>
#include <sys/list.h>
#include <sys/math.h>
#include <sys/rings.h>
#include <time.h>

typedef struct async_ctx async_ctx_t;
typedef struct process process_t;

/**
 * @brief Asynchronous Request Primitive
 * @defgroup kernel_sync_request Request
 * @ingroup kernel_sync
 *
 * The request primitive is designed to be generic enough to be used by any system in the kernel, however it is
 * primarily used by the asynchronous rings system.
 *
 * @warning The request system is not thread-safe, it is the responsibility of the caller to ensure proper
 * synchronization.
 *
 * @see kernel_sync_async for the asynchronous rings system.
 *
 * ## Completion Callback
 *
 * The `complete()` callback should be called when the request has been completed, the `complete()` implementation does
 * not need to guarantee that the request structure will remain valid after a call to this function.
 *
 * Generally, the completion callback should be implemented by the creator of the request while the `cancel()` callback
 * is implemented by the subsystem processing the request.
 *
 * ## Cancellation Callback
 *
 * The optional `cancel()` callback is called when attempting to cancel an in-progress request or when its deadline
 * expires, if the request cannot be cancelled, the callback should return `false`, otherwise `true`.
 *
 * @{
 */

/**
 * @brief Request ID type.
 */
typedef uint16_t request_id_t;

/**
 * @brief The maximum id value for requests.
 */
#define REQUEST_ID_MAX UINT16_MAX

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
 * @note The ordering here is important to avoid padding and keeping the full `request_t` structure at 128 bytes.
 *
 * @param _type The type of the request structure.
 * @param _resultType The type of the request result.
 */
#define REQUEST_COMMON(_type, _resultType) \
    static_assert(sizeof(_resultType) == sizeof(uint64_t), "result type must be 64 bits"); \
    list_entry_t entry; \
    list_entry_t timeoutEntry; \
    void (*complete)(_type*); \
    bool (*cancel)(_type*); \
    union { \
        clock_t deadline; \
        clock_t timeout; \
    }; \
    request_id_t index; \
    request_id_t next; \
    cpu_id_t cpu; \
    uint8_t flags; \
    uint8_t type; \
    uint8_t err; \
    void* data; \
    _resultType result;

/**
 * @brief Generic request structure.
 * @struct request_t
 *
 * @warning Due to optimization for the request pools, no request structure should be
 * larger than this structure.
 */
typedef struct request
{
    REQUEST_COMMON(struct request, uint64_t);
    uint64_t _raw[SEQ_MAX_ARGS]; ///< Should be used by requests to store data.
} request_t;

static_assert(sizeof(request_t) == 128, "request_t is not 128 bytes");

/**
 * @brief Request pool structure.
 * @struct request_pool_t
 */
typedef struct request_pool
{
    void* ctx;
    size_t used;
    list_t free;
    request_t requests[];
} request_pool_t;

/**
 * @brief Allocate a new request pool.
 *
 * @param size The amount of requests to allocate.
 * @param ctx The context of the request pool.
 * @return On success, a pointer to the new request pool. On failure, `NULL` and `errno` is set.
 */
request_pool_t* request_pool_new(size_t size, void* ctx);

/**
 * @brief Free a request pool.
 *
 * @param pool Pointer to the request pool to free.
 */
void request_pool_free(request_pool_t* pool);

/**
 * @brief Retrieve the request pool that a request was allocated from.
 *
 * @param request Pointer to the request.
 * @return Pointer to the request pool.
 */
static inline request_pool_t* request_get_pool(request_t* request)
{
    return CONTAINER_OF(request, request_pool_t, requests[request->index]);
}

/**
 * @brief Retrieve the context of the request pool that a request was allocated from.
 *
 * @param request Pointer to the request.
 * @return Pointer to the context.
 */
static inline void* request_get_ctx(request_t* request)
{
    return request_get_pool(request)->ctx;
}

/**
 * @brief Retrieve the next request and clear the next field.
 *
 * @param request Pointer to the current request.
 * @return Pointer to the next request, or `NULL` if there is no next request.
 */
static inline request_t* request_next(request_t* request)
{
    request_pool_t* pool = request_get_pool(request);
    if (request->next == REQUEST_ID_MAX)
    {
        return NULL;
    }

    request_t* next = &pool->requests[request->next];
    request->next = REQUEST_ID_MAX;
    return next;
}

/**
 * @brief Allocate a new request from a pool.
 *
 * The pool that the request was allocated from, and its context, can be retrieved using the `request_get_pool()`
 * function.
 *
 * @param pool Pointer to the request pool.
 * @return On success, a pointer to the allocated request. On failure, `NULL`.
 */
static inline request_t* request_new(request_pool_t* pool)
{
    if (list_is_empty(&pool->free))
    {
        return NULL;
    }

    pool->used++;
    return CONTAINER_OF(list_pop_back(&pool->free), request_t, entry);
}

/**
 * @brief Free a request back to its pool.
 *
 * @param request Pointer to the request to free.
 */
static inline void request_free(request_t* request)
{
    request_pool_t* pool = request_get_pool(request);
    pool->used--;
    list_push_back(&pool->free, &request->entry);
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
            (_request)->flags |= REQUEST_TIMEOUT; \
            request_timeout_add((request_t*)(_request)); \
        } \
        result; \
    })

/**
 * @brief Macro to delay the completion of a request.
 *
 * @param _request Pointer to the request to delay.
 * @param _queue Pointer to a list to add the request to.
 */
#define REQUEST_DELAY(_request, _queue) \
    ({ \
        list_push_back(_queue, &(_request)->entry); \
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