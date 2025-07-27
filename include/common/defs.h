#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ALIGNED(alignment) __attribute__((aligned(alignment)))
#define PACKED __attribute__((packed))
#define NORETURN __attribute__((noreturn))
#define NOINLINE __attribute__((noinline))

/**
 * @brief GCC const function attribute.
 * @ingroup kernel
 *
 * The `CONST_FUNC` attribute tells gcc that the fuction with said attribute only depends on the arguments passed to it,
 * and will never access global variables.
 *
 */
#define CONST_FUNC __attribute__((const))

/**
 * @brief GCC
 * @ingroup kernel
 *
 * The `PURE_FUNC` attribute tells gcc that the function with said attribute only depends on the arguments passed to it
 * and potentially global variables.
 *
 */
#define PURE_FUNC __attribute__((pure))

#define CONCAT(a, b) CONCAT_INNER(a, b)
#define CONCAT_INNER(a, b) a##b
