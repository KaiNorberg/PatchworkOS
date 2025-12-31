#pragma once

#include <kernel/fs/file.h>
#include <kernel/sync/lock.h>
#include <kernel/utils/ref.h>

#include <sys/list.h>

typedef struct process process_t;

typedef struct group group_t;

/**
 * @brief Process groups.
 * @defgroup kernel_proc_group Process groups
 * @ingroup kernel_proc
 *
 * Process groups allow related processes to be grouped together, enabling operations such as sending
 * notes to all processes within a group.
 *
 * As an example, if a user wishes to terminate a shell they most likely additionally want to terminate all
 * child processes of that shell. By placing all such processes in the same group, a single "terminate" note
 * can be sent to the entire group.
 *
 * @{
 */

/**
 * @brief Group member structure.
 * @struct group_member_t
 *
 * Stored in each process.
 */
typedef struct
{
    list_entry_t entry;
    group_t* group;
    lock_t lock;
} group_member_t;

/**
 * @brief Process group structure.
 * @struct group_t
 */
typedef struct group
{
    ref_t ref;
    list_t processes;
    lock_t lock;
} group_t;

/**
 * @brief Initializes a group member.
 *
 * @param member The group member to initialize.
 * @param group A member storing the group to add the new member to, or `NULL` to create a new group.
 * @return On success, `0`. On failure, `ERR` and `errno` is set to:
 */
uint64_t group_member_init(group_member_t* member, group_member_t* group);

/**
 * @brief Deinitializes a group member.
 *
 * @param member The group member to deinitialize.
 */
void group_member_deinit(group_member_t* member);

/**
 * @brief Retrieve the group of a group member.
 * 
 * It is the responsibility of the caller to use `UNREF()` or `UNREF_DEFER()` on the returned group when it is no longer needed.
 * 
 * @param member The group member.
 * @return On success, a reference to the group. On failure, `NULL` and `errno` is set to:
 * - `EINVAL`: Invalid parameters.
 * - `ESRCH`: The member is not part of any group.
 */
group_t* group_get(group_member_t* member);

/**
 * @brief Joins a process to a specific group.
 *
 * If the member is already in a group it will be removed from that group first.
 *
 * @param group The group to join.
 * @param member The group member of the process to add to the group.
 */
void group_add(group_t* group, group_member_t* member);

/**
 * @brief Removes a process from its group.
 *
 * If the group becomes empty after removing the process, it will be freed.
 *
 * @param member The group member of the process to remove from its group, or `NULL` for no-op.
 */
void group_remove(group_member_t* member);

/**
 * @brief Sends a note to all processes in the group of the specified member.
 *
 * @param member The member within the group to send the note to.
 * @param note The note string to send.
 * @return On success, `0`. On failure, `ERR` and `errno` is set to:
 * - `EINVAL`: Invalid parameters.
 * - Other values from `thread_send_note()`.
 */
uint64_t group_send_note(group_member_t* member, const char* note);

/** @} */