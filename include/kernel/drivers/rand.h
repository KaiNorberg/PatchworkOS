#pragma once

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Random Number Generator
 * @defgroup kernel_drivers_rand Random Number Generator
 * @ingroup kernel_drivers
 *
 * The random number generator driver provides functions to generate random numbers for use in the kernel.
 *
 * @see [RDRAND Instruction](https://www.felixcloutier.com/x86/rdrand)
 *
 * @{
 */

/**
 * @brief CPU random number generator context.
 * @struct rand_cpu_t
 */
typedef struct
{
    bool rdrandAvail; ///< If set, the `RDRAND` instruction is available and working.
} rand_cpu_t;

/**
 * @brief Initializes the random number generator.
 */
void rand_cpu_init(rand_cpu_t* ctx);

/**
 * @brief Fills a buffer with random bytes.
 *
 * If the RDRAND instruction is available and working, it will be used. Otherwise, a fallback time based RNG will be
 * used.
 *
 * @param buffer A pointer to the buffer to fill.
 * @param size The number of bytes to fill.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t rand_gen(void* buffer, uint64_t size);

/**
 * @brief Generates a random 32-bit unsigned integer using the RDRAND instruction.
 *
 * @param value A pointer to store the generated random value.
 * @param retries The number of retries to attempt if RDRAND fails.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
extern uint64_t rdrand_do(uint32_t* value, uint8_t retries);

/** @} */
