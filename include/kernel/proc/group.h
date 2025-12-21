#pragma once

#include <kernel/sync/lock.h>
#include <kernel/utils/map.h>

#include <sys/list.h>
#include <sys/proc.h>

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
} group_member_t;

/**
 * @brief Process group structure.
 * @struct group_t
 */
typedef struct group
{
    map_entry_t mapEntry;
    gid_t id;
    list_t processes;
} group_t;

/**
 * @brief Initializes a group member.
 *
 * @param member The group member to initialize.
 */
void group_member_init(group_member_t* member);

/**
 * @brief Deinitializes a group member.
 *
 * @param member The group member to deinitialize.
 */
void group_member_deinit(group_member_t* member);

/**
 * @brief Adds a process to a group.
 *
 * @param gid The group ID to add the process to, or `GID_NONE` to create a new group.
 * @param member The group member of the process to add to the group.
 * @return On success, `0`. On failure, `ERR` and errno is set to:
 * - `EINVAL`: Invalid parameters.
 * - `ENOENT`: The specified group does not exist.
 * - `EBUSY`: The member is already part of a group.
 * - `ENOMEM`: Out of memory.
 */
uint64_t group_add(gid_t gid, group_member_t* member);

/**
 * @brief Removes a process from its group.
 *
 * If the group becomes empty after removing the process, it will be freed.
 *
 * @param member The group member of the process to remove from its group, or `NULL` for no-op.
 */
void group_remove(group_member_t* member);

/**
 * @brief Gets the ID of the group of the specified member.
 *
 * @param member The group member to get the group ID from.
 * @return The group ID or `GID_NONE` if the member is not part of a group.
 */
gid_t group_get_id(group_member_t* member);

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