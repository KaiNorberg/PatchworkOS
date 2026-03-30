#pragma once

#include <kernel/fs/file.h>
#include <kernel/sync/lock.h>
#include <kernel/utils/ref.h>

#include <libc/list.h>

typedef struct process process_t;

typedef struct job job_t;

/**
 * @brief Process jobs.
 * @defgroup kernel_proc_job Process Jobs
 * @ingroup kernel_proc
 *
 * Jobs represent a collection of processes. Each job has a parent and children, along with a leader process (being the
 * process that first created the job).
 *
 * @{
 */

/**
 * @brief Job member structure.
 * @struct job_member_t
 *
 * Stored in each process.
 */
typedef struct
{
    list_entry_t entry;
    job_t* job;
    lock_t lock;
} job_member_t;

/**
 * @brief Process job structure.
 * @struct job_t
 */
typedef struct job
{
    ref_t ref;
    list_t members;
    lock_t lock;
    job_t* parent;
    list_t children;
    list_entry_t sibling;
    job_member_t* leader;
} job_t;

/**
 * @brief Initializes a job member.
 *
 * @param member The job member to initialize.
 */
void job_member_init(job_member_t* member);

/**
 * @brief Deinitializes a job member.
 *
 * @param member The job member to deinitialize.
 */
void job_member_deinit(job_member_t* member);

/**
 * @brief Creates a new job.
 *
 * It is the responsibility of the caller to use `UNREF()` or `UNREF_DEFER()` on the returned job when it is no longer
 * needed.
 *
 * @param out Output pointer for the new job.
 * @param parent The parent job, or `NULL`.
 * @return An appropriate status value.
 */
status_t job_new(job_t** out, job_t* parent);

/**
 * @brief Retrieve the job of a job member.
 *
 * It is the responsibility of the caller to use `UNREF()` or `UNREF_DEFER()` on the returned job when it is no longer
 * needed.
 *
 * @param member The job member.
 * @return On success, a reference to the job. On failure, `NULL`.
 */
job_t* job_get(job_member_t* member);

/**
 * @brief Joins a member to a specific job.
 *
 * If the member is already in a job it will be removed from that job first.
 *
 * If the job does not contain any members, the member will be set as the leader.
 *
 * @param job The job to join.
 * @param member The job member of the process to add to the job.
 */
void job_join(job_t* job, job_member_t* member);

/**
 * @brief Removes a member from its job.
 *
 * If the member is the leader of its job, the first member within the job will be selected as the new leader.
 *
 * @param member The job member of the process to remove from its job, or `NULL` for no-op.
 */
void job_leave(job_member_t* member);

/**
 * @brief Checks if a job is accessible from another job.
 *
 * A job is accessible if it is the same job or a descendant of the job.
 *
 * @param job The job that is performing the access.
 * @param target The target job to check accessibility for.
 * @return `true` if the target job is accessible, `false` otherwise.
 */
bool job_is_accessible(job_t* job, job_t* target);

/**
 * @brief Checks if a job member is the leader of its job.
 *
 * @param job The job to check.
 * @param member The job member to check.
 * @return `true` if the member is the leader, `false` otherwise.
 */
bool job_is_leader(job_t* job, job_member_t* member);

/**
 * @brief Sends a note to all processes in a job.
 *
 * @param job The job to send the note to.
 * @param note The note string to send.
 */
void job_send_note(job_t* job, const char* note);

/** @} */