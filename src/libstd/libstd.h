#pragma once

/**
 * @brief Standard Library
 * @defgroup libstd Standard Library
 *
 * This is a custom superset of the C standard library, this is not POSIX, extensions can be found within the sys
 * folder, this does potentially limit compatibility with most software.
 *
 * The PDCLIB was heavily used when making this library.
 *
 * The standard library code is shared between the kernel and user space however the physical binary is not, they will
 * compile their own versions of the standard library, in practice this is just to reduce code duplication.
 *
 * All non public definitions must be prefixed with `_` for example `_heap_header_t`.
 *
 * @see [PDCLIB Repo](https://github.com/DevSolar/pdclib)
 *
 */
