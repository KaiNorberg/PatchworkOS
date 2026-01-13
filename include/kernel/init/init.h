#pragma once

#include <kernel/cpu/cpu.h>

/**
 * @brief Initialization and `kmain()`.
 * @defgroup kernel_init Initialization
 * @ingroup kernel
 *
 * @{
 */

/**
 * @brief Early kernel initialization.
 *
 * This will do the absolute minimum to get the scheduler running.
 *
 * Having the scheduler running lets us load the boot thread which will jump to `kmain()` where we can do the rest
 * of the kernel initialization.
 *
 * Will be called in the `_start()` function found in `start.S` with interrupts disabled.
 */
_NORETURN void init_early(void);

/**
 * @brief Kernel main function.
 *
 * This is the entry point for the boot thread. When `init_early()` jumps to the boot thread we will end up here. We
 * then perform the rest of the kernel initialization here and start the init process.
 *
 * Will never return, the boot thread will exit itself when done.
 */
_NORETURN void kmain(void);

/**
 * @brief Calls all functions in the `.init_array` section and initializes percpu variables for the current module.
 */
#define INIT_CALL() \
    do \
    { \
        extern void* _initArrayStart; \
        extern void* _initArrayEnd; \
        for (void** func = &_initArrayStart; func < &_initArrayEnd; func++) \
        { \
            void (*initFunc)(void) = (void (*)(void)) * func; \
            initFunc(); \
        } \
        PERCPU_INIT(); \
    } while (0)

/**
 * @brief Calls all functions in the `.finit_array` section and deinitializes percpu variables for the current module.
 */
#define FINIT_CALL() \
    do \
    { \
        PERCPU_FINIT(); \
        extern void* _finitArrayStart; \
        extern void* _finitArrayEnd; \
        for (void** func = &_finitArrayStart; func < &_finitArrayEnd; func++) \
        { \
            void (*finitFunc)(void) = (void (*)(void)) * func; \
            finitFunc(); \
        } \
    } while (0)

/** @} */
