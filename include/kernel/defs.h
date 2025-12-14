#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Defines.
 * @defgroup kernel_defs Kernel Defines
 * @ingroup kernel
 *
 * @{
 */

/**
 * @brief GCC aligned attribute.
 *
 * Tells the compiler to align a variable or structure field to the specified byte alignment.
 *
 * Usefull for caching or hardware requirements.
 */
#define ALIGNED(alignment) __attribute__((aligned(alignment)))

/**
 * @brief GCC packed attribute.
 *
 * Tells the compiler to pack a structure, meaning there will be no padding between members.
 *
 * Needed for most hardware structures.
 *
 */
#define PACKED __attribute__((packed))

/**
 * @brief GCC noreturn function attribute.
 *
 * Tells the compiler that the fuction with said attribute will never return.
 */
#define NORETURN __attribute__((noreturn))

/**
 * @brief GCC noinline function attribute.
 *
 * Tells the compiler to never inline the function with said attribute.
 *
 */
#define NOINLINE __attribute__((noinline))

/**
 * @brief GCC const function attribute.
 *
 * Tells the compiler that the fuction with said attribute only depends on the arguments passed to it, and will never
 * access global variables.
 *
 */
#define CONST_FUNC __attribute__((const))

/**
 * @brief GCC
 *
 * The `PURE` attribute tells gcc that the function with said attribute only depends on the arguments passed to it
 * and potentially global variables.
 *
 */
#define PURE __attribute__((pure))

/**
 * @brief Concatenates two tokens.
 *
 * This macro concatenates two tokens `a` and `b` into a single token.
 *
 * @param a The first token.
 * @param b The second token.
 * @return The concatenated token.
 */
#define CONCAT(a, b) CONCAT_INNER(a, b)

/**
 * @brief Inner helper macro for token concatenation.
 */
#define CONCAT_INNER(a, b) a##b

/**
 * @brief The size of the red zone in bytes.
 *
 * The red zone is a region of memory below the stack pointer that is reserved and should not be modified by
 * interrupt handlers or signal handlers. The compiler uses this area for temporary storage for the purpose of optimization.
 */
#define RED_ZONE_SIZE 128

/** @} */
