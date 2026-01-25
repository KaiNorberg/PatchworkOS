#include <kernel/proc/reaper.h>

#include <kernel/log/panic.h>
#include <kernel/proc/process.h>
#include <kernel/sched/clock.h>
#include <kernel/sched/sched.h>
#include <kernel/sched/thread.h>
#include <kernel/sync/lock.h>
#include <kernel/sync/rcu.h>

static list_t zombies = LIST_CREATE(zombies);
static clock_t nextReaperTime = CLOCKS_NEVER;
static lock_t reaperLock = LOCK_CREATE();

static void reaper_thread(void* arg)
{
    UNUSED(arg);

    while (1)
    {
        sched_nanosleep(CONFIG_PROCESS_REAPER_INTERVAL);

        lock_acquire(&reaperLock);
        clock_t uptime = clock_uptime();
        if (uptime < nextReaperTime)
        {
            lock_release(&reaperLock);
            continue;
        }
        nextReaperTime = CLOCKS_NEVER;

        list_t localZombies = LIST_CREATE(localZombies);

        while (!list_is_empty(&zombies))
        {
            process_t* process = CONTAINER_OF(list_pop_front(&zombies), process_t, zombieEntry);

            RCU_READ_SCOPE();
            if (process_rcu_thread_count(process) > 0)
            {
                list_push_back(&zombies, &process->zombieEntry);
                continue;
            }

            list_push_back(&localZombies, &process->zombieEntry);
        }
        lock_release(&reaperLock);

        while (!list_is_empty(&localZombies))
        {
            process_t* process = CONTAINER_OF(list_pop_front(&localZombies), process_t, zombieEntry);
            process_remove(process);
            UNREF(process);
        }
    }
}

void reaper_init(void)
{
    if (thread_kernel_create(reaper_thread, NULL) == _FAIL)
    {
        panic(NULL, "Failed to create process reaper thread");
    }
}

void reaper_push(process_t* process)
{
    lock_acquire(&reaperLock);
    list_push_back(&zombies, &REF(process)->zombieEntry);
    nextReaperTime = clock_uptime() + CONFIG_PROCESS_REAPER_INTERVAL;
    lock_release(&reaperLock);
}