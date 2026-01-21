#pragma once

#include <kernel/io/irp.h>

#include <sys/ioring.h>

/**
 * @brief I/O Request Packet verbs.
 * @defgroup kernel_io_verb I/O Request Packet Verbs
 * @ingroup kernel_io
 *
 *
 * @{
 */

typedef void (*verb_handler_t)(irp_t* irp);

typedef struct verb_table
{
    verb_handler_t handlers[VERB_MAX];
} verb_table_t;

/** @} */