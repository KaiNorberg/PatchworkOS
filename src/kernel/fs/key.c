#include <kernel/fs/key.h>

#include <kernel/cpu/syscall.h>
#include <kernel/drivers/rand.h>
#include <kernel/log/panic.h>
#include <kernel/sched/clock.h>
#include <kernel/sched/thread.h>
#include <kernel/sched/timer.h>
#include <kernel/sched/wait.h>
#include <kernel/sync/rwlock.h>

#include <errno.h>
#include <kernel/utils/map.h>
#include <stdlib.h>
#include <sys/list.h>

static map_t keyMap = MAP_CREATE();
static list_t keyList = LIST_CREATE(keyList);
static lock_t keyLock = LOCK_CREATE();

static key_t key_generate(void)
{
    key_t key;
    map_key_t mapKey;
    do
    {
        if (rand_gen(&key, sizeof(key)) == ERR)
        {
            panic(NULL, "failed to generate random key");
        }
        mapKey = map_key_buffer(&key, sizeof(key));
    } while (map_get(&keyMap, &mapKey) != NULL);

    return key;
}

static void key_timer_handler(interrupt_frame_t* frame, cpu_t* self)
{
    (void)frame;
    (void)self;

    clock_t uptime = clock_uptime();
    LOCK_SCOPE(&keyLock);

    key_entry_t* entry;
    key_entry_t* temp;
    LIST_FOR_EACH_SAFE(entry, temp, &keyList, entry)
    {
        if (entry->expiry > uptime)
        {
            timer_set(uptime, entry->expiry);
            break;
        }

        map_remove(&keyMap, &entry->mapEntry);
        list_remove(&keyList, &entry->entry);
        UNREF(entry->file);
        free(entry);
    }
}

uint64_t key_share(key_t* key, file_t* file, clock_t timeout)
{
    if (key == NULL || file == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    key_entry_t* entry = malloc(sizeof(key_entry_t));
    if (entry == NULL)
    {
        return ERR;
    }
    list_entry_init(&entry->entry);
    map_entry_init(&entry->mapEntry);
    entry->file = REF(file);
    entry->expiry = CLOCKS_DEADLINE(timeout, clock_uptime());

    LOCK_SCOPE(&keyLock);

    entry->key = key_generate();
    
    map_key_t mapKey = map_key_buffer(&entry->key, sizeof(entry->key));
    if (map_insert(&keyMap, &mapKey, &entry->mapEntry) == ERR)
    {
        UNREF(entry->file);
        free(entry);
        return ERR;
    }

    *key = entry->key;
    if (list_length(&keyList) == 0)
    {
        list_push_back(&keyList, &entry->entry);
        timer_set(clock_uptime(), entry->expiry);
        return 0;
    }

    key_entry_t* other;
    LIST_FOR_EACH(other, &keyList, entry)
    {
        if (entry->expiry < other->expiry)
        {
            list_prepend(&keyList, &other->entry, &entry->entry);
            timer_set(clock_uptime(), entry->expiry);
            return 0;
        }
    }

    list_push_back(&keyList, &entry->entry);
    return 0;
}

SYSCALL_DEFINE(SYS_SHARE, uint64_t, key_t* key, fd_t fd, clock_t timeout)
{
    thread_t* thread = sched_thread();
    process_t* process = thread->process;

    if (space_check_access(&process->space, key, sizeof(*key)) == ERR)
    {
        errno = EFAULT;
        return ERR;
    }

    file_t* file = file_table_get(&process->fileTable, fd);
    if (file == NULL)
    {
        return ERR;
    }
    UNREF_DEFER(file);

    key_t keyCopy;
    if (key_share(&keyCopy, file, timeout) == ERR)
    {
        return ERR;
    }

    if (thread_copy_to_user(thread, key, &keyCopy, sizeof(keyCopy)) == ERR)
    {
        UNREF(key_claim(&keyCopy));
        return ERR;
    }
    return 0;
}

file_t* key_claim(key_t* key)
{
    if (key == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    LOCK_SCOPE(&keyLock);
    map_key_t mapKey = map_key_buffer(key, sizeof(*key));
    map_entry_t* mapEntry = map_get_and_remove(&keyMap, &mapKey);
    if (mapEntry == NULL)
    {
        errno = ENOENT;
        return NULL;
    }

    key_entry_t* entry = CONTAINER_OF(mapEntry, key_entry_t, mapEntry);
    list_remove(&keyList, &entry->entry);

    file_t* file = entry->file;
    free(entry);
    return file;
}

SYSCALL_DEFINE(SYS_CLAIM, fd_t, key_t* key)
{
    thread_t* thread = sched_thread();
    process_t* process = thread->process;

    key_t keyCopy;
    if (thread_copy_from_user(thread, &keyCopy, key, sizeof(keyCopy)) == ERR)
    {
        errno = EFAULT;
        return ERR;
    }

    file_t* file = key_claim(&keyCopy);
    if (file == NULL)
    {
        return ERR;
    }
    UNREF_DEFER(file);

    return file_table_open(&process->fileTable, file);
}
