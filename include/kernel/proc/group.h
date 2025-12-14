#pragma once

#include <kernel/sync/lock.h>
#include <kernel/utils/map.h>

#include <sys/proc.h>
#include <sys/list.h>

typedef struct process process_t;

/** 
 * @brief Process groups.
 * @defgroup kernel_proc_group Process groups
 * @ingroup kernel_proc
 * 
 * Process groups allow related processes to be grouped together, enabling operations such as sending
 * notes to all processes within a group.
 * 
 * As an example, if a user wishes to terminate a shell they most likely additionally want to terminate all
 * child processes started by that shell. By placing all such processes in the same group, a single "terminate" note
 * can be sent to the entire group, effectively terminating all related processes.
 * 
 * @{
 */

/**
 * @brief Process group structure.
 * @struct group_t
 */
typedef struct group
{
    map_entry_t mapEntry;
    gid_t id;
    list_t processes;
    lock_t lock;
} group_t;

/**
 * @brief Adds a process to a group.
 * 
 * @param gid The group ID to add the process to, or `GID_NONE` to create a new group.
 * @param process The process to add to the group.
 * @return On success, the group the process was added to. On failure, `NULL` and errno is set to:
 * - `EINVAL`: Invalid parameters.
 * - `ENOENT`: The specified group does not exist.
 * - `ENOMEM`: Out of memory.
 */
group_t* group_add(gid_t gid, process_t* process);

/**
 * @brief Removes a process from a group.
 * 
 * If the group becomes empty after removing the process, it will be freed.
 * 
 * @param group The group to remove the process from, or `NULL` for no-op.
 * @param process The process to remove from the group, or `NULL` for no-op.
 */
void group_remove(group_t* group, process_t* process);

/** @} */