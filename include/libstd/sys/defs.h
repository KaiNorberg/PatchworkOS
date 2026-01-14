#ifndef _SYS_DEFS_H
#define _SYS_DEFS_H 1

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Defines.
 * @ingroup libstd
 * @defgroup libstd_sys_defs Defines
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
 * interrupt handlers or signal handlers. The compiler uses this area for temporary storage for the purpose of
 * optimization.
 */
#define RED_ZONE_SIZE 128

/**
 * @brief Mark a variable as unused.
 *
 * This macro marks a variable as unused to prevent compiler warnings about unused variables.
 *
 * @param x The variable to mark as unused.
 */
#define UNUSED(x) (void)(x)

/**
 * @brief GCC unused function attribute.
 *
 * Tells the compiler that the function with said attribute might be unused, preventing warnings.
 */
#define UNUSED_FUNC __attribute__((unused))

/**
 * @brief Get the number of elements in a static array.
 *
 * @param x The array.
 * @return The number of elements in the array.
 */
#define ARRAY_SIZE(x) ((size_t)(sizeof(x) / sizeof((x)[0])))

/**
 * @brief Mark a condition as likely.
 *
 * This macro marks a condition as likely to help the compiler optimize branch prediction.
 *
 * @param x The condition.
 */
#define LIKELY(x) __builtin_expect(!!(x), 1)

/**
 * @brief Mark a condition as unlikely.
 *
 * This macro marks a condition as unlikely to help the compiler optimize branch prediction.
 *
 * @param x The condition.
 */
#define UNLIKELY(x) __builtin_expect(!!(x), 0)

/**
 * @brief GCC constructor function attribute.
 *
 * Will add the function to the `.init_array` section with the given priority, the function can then be called using
 * `INIT_CALL()`.
 *
 * Functions with a higher priority number are called last.
 *
 * @param priority The priority of the constructor function.
 */
#define CONSTRUCTOR(priority) __attribute__((used, constructor(priority)))

/**
 * @brief GCC destructor function attribute.
 *
 * Will add the function to the `.finit_array` section with the given priority, the function can then be called using
 * `FINIT_CALL()`.
 *
 * Functions with a higher priority number are called last.
 *
 * @param priority The priority of the destructor function.
 */
#define DESTRUCTOR(priority) __attribute__((used, destructor(priority)))

/**
 * @brief Inline assembly macro.
 *
 * @param ... The assembly code to embed.
 */
#define ASM(...) __asm__ volatile (__VA_ARGS__)

/** @} */

#endif