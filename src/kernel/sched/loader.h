#pragma once

#include "defs.h"
#include "sched/thread.h"

#include <stdarg.h>

/**
 * @brief Program loading and user stack management.
 * @defgroup kernel_sched_loader Program Loader
 * @ingroup kernel_sched
 *
 * The loader is responsible for loading programs from the file system and managing user stacks.
 *
 * ## User stacks
 *
 * The address of a threads user stack is decided based of its id. The user stack of the thread with id 0 is located att
 * the very top of a processes address space, the user stack is `CONFIG_MAX_USER_STACK_PAGES` pages long, and below it
 * is a guard page, which is always unmapped. The guard page is intended to detect stack overflows by causing page
 * faults when a thread tries to access it. Below that guard page is the user stack of the thread with id 1, below that
 * is its guard page, it then continues like that for however many threads there are.
 *
 * When a thread is first created only the page att the very top of the user stack is mapped, and when pagefaults due to
 * non present pages occur within the threads user stack additional pages will be mapped to where the page fault
 * occoured, check the `trap_handler()` for more information.
 *
 * When a thread is freed, its entire user stack will also be unmapped.
 *
 */

/**
 * @brief Retrieves the top of a threads user stack.
 * @ingroup kernel_sched_loader
 *
 * The `LOADER_USER_STACK_TOP()` macro retrieves the address of the top of a threads user stack given the id of the
 * thread.
 *
 * Note that in x86 the push operation moves the stack pointer first and then writes to the location of the stack
 * pointer, that means that even if this address is not inclusive the stack pointer of a thread should be initalized to
 * the result of the `LOADER_USER_STACK_TOP()` macro.
 *
 * @return The address of the top of the user stack, this address is not inclusive and always page aligned.
 */
#define LOADER_USER_STACK_TOP(tid) (VMM_LOWER_HALF_END - (((CONFIG_MAX_USER_STACK_PAGES + 1) * PAGE_SIZE) * (tid)))

/**
 * @brief Retrieves the bottom of a threads user stack.
 * @ingroup kernel_sched_loader
 *
 * The `LOADER_USER_STACK_BOTTOM()` macro retrieves the address of the bottom of a threads user stack given the id of
 * the thread.
 *
 * @return The address of the bottom of the user stack, this address is inclusive and always page aligned.
 */
#define LOADER_USER_STACK_BOTTOM(tid) (LOADER_USER_STACK_TOP(tid) - (CONFIG_MAX_USER_STACK_PAGES * PAGE_SIZE))

/**
 * @brief Retrieves the top of a threads guard page.
 * @ingroup kernel_sched_loader
 *
 * The `LOADER_GUARD_PAGE_TOP()` macro retrieves the address of the top of a threads guard page given the id of the
 * thread.
 *
 * @return The address of the top of the guard page, this address is not inclusive and always page aligned.
 */
#define LOADER_GUARD_PAGE_TOP(tid) (LOADER_USER_STACK_BOTTOM(tid) - 1)

/**
 * @brief Retrieves the bottom of a threads guard page.
 * @ingroup kernel_sched_loader
 *
 * The `LOADER_GUARD_PAGE_BOTTOM()` macro retrieves the address of the bottom of a threads guard page given the id of
 * the thread.
 *
 * @return The address of the bottom of the guard page, this address is inclusive and always page aligned.
 */
#define LOADER_GUARD_PAGE_BOTTOM(tid) (LOADER_GUARD_PAGE_TOP(tid) - PAGE_SIZE)

/**
 * @brief Performs the initial jump to userspace.
 * @ingroup kernel_sched_loader
 *
 * The `loader_jump_to_user_space()` function uses the `iretq` instruction to jump to userspace, passing process
 * arguments and setting the stack and program pointers.
 *
 * @param argc The length of the argument array to pass to userspace.
 * @param argv The argument buffer to pass to userspace.
 * @param rsp The new stack pointer.
 * @param rip The new instruction pointer.
 * @return NORETURN
 */
extern NORETURN void loader_jump_to_user_space(int argc, char** argv, void* rsp, void* rip);

/**
 * @brief Spawns a child process from an executable file.
 * @ingroup kernel_sched_loader
 *
 * The `loader_spawn()` function loads and executes a program from the specified path found in `argv[0]`.
 *
 * @param argv A null-terminated array of strings storing the arguments to be passed to usespace and the executable to
 * be loaded in `argv[0]`.
 * @param priority The priority of the main thread within the child process.
 * @param cwd The current working directory for the child process, if `cwd` is equal to null then the child inherits the
 * working directory of the parent.
 * @return On success, returns the main thread of the child process. On failure, returns `NULL` and the
 * running threads `thread_t::error` member is set.
 */
thread_t* loader_spawn(const char** argv, priority_t priority, const path_t* cwd);

/**
 * @brief Creates a new thread within an existing process.
 * @ingroup kernel_sched_loader
 *
 * The `loader_thread_create()` function creates a new thread within a specified parent process. This new thread will
 * begin execution at the provided entry point with the given argument as the first argument to the entry point.
 *
 * @param parent The parent process for the new thread.
 * @param priority The priority of the new thread.
 * @param entry The entry point address for the new thread.
 * @param arg An argument to pass to the entry point.
 * @return On success, returns the newly created thread. On failure, returns `NULL` and the running threads
 * `thread_t::error` member is set.
 */
thread_t* loader_thread_create(process_t* parent, priority_t priority, void* entry, void* arg);