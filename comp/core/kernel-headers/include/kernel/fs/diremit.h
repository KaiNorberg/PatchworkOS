#pragma once

#include <kernel/io/irp.h>
#include <sys/status.h>

/**
 * @brief Helpers for implementing directory reads.
 * @defgroup kernel_fs_diremit Directory Emitter
 * @ingroup kernel_fs
 *
 * @{
 */

/**
 * @brief Directory read emitter.
 * @struct diremit_t
 */
typedef struct diremit
{
    irp_t* irp;
    size_t offset;
    size_t currentPos;
    size_t bytes;
    status_t status;
} diremit_t;

/**
 * @brief Initialize a directory emitter.
 *
 * This should be called at the beginning of a directory read operation, before emitting any entries with `diremit()`.
 *
 * @param emit The emitter to initialize.
 * @param irp The read IRP associated with the directory read operation.
 */
static inline void diremit_begin(diremit_t* emit, irp_t* irp)
{
    emit->irp = irp;
    emit->offset = *irp_current(irp)->read.offset;
    emit->currentPos = 0;
    emit->bytes = 0;
    emit->status = INFO(VFS, EOF);
}

/**
 * @brief Finalize a directory read context.
 *
 * This should be called at the end of a directory read operation, after emitting entries with `diremit()`.
 *
 * @param emit The emitter to finalize.
 * @return An appropriate status value.
 */
static inline status_t diremit_end(diremit_t* emit)
{
    status_t status = emit->status;

    if (IS_INFO(status))
    {
        irp_frame_t* frame = irp_current(emit->irp);
        *frame->read.offset += emit->bytes;
        emit->irp->result = emit->bytes;
    }

    emit->irp = NULL;
    emit->offset = 0;
    emit->currentPos = 0;
    emit->bytes = 0;
    emit->status = OK;

    return status;
}

/**
 * @brief Helper to emit a directory entry.
 *
 * @param emit The directory emitter.
 * @param name The name of the entry.
 * @return `true` if the entry was emitted successfully or skipped, `false` if the buffer is full or an error occurred.
 */
bool diremit(diremit_t* emit, const char* name);

/** @} */