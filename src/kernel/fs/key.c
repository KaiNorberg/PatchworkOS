#include <kernel/fs/key.h>

#include <kernel/cpu/syscalls.h>
#include <kernel/drivers/rand.h>
#include <kernel/log/panic.h>
#include <kernel/sched/sys_time.h>
#include <kernel/sched/thread.h>
#include <kernel/sched/timer.h>
#include <kernel/sync/rwlock.h>

#include <errno.h>
#include <kernel/utils/map.h>
#include <stdlib.h>

static map_t keyMap;
static list_t keyList;
static rwlock_t keyLock;

static key_t key_generate(void)
{
    RWLOCK_READ_SCOPE(&keyLock);

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

    clock_t now = sys_time_uptime();
    RWLOCK_WRITE_SCOPE(&keyLock);

    key_entry_t* entry;
    key_entry_t* temp;
    LIST_FOR_EACH_SAFE(entry, temp, &keyList, entry)
    {
        if (entry->expiry > now)
        {
            break;
        }

        map_remove(&keyMap, &entry->mapEntry);
        list_remove(&keyList, &entry->entry);
        DEREF(entry->file);
        free(entry);
    }
}

void key_init(void)
{
    map_init(&keyMap);
    list_init(&keyList);
    rwlock_init(&keyLock);
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
    entry->key = key_generate();
    entry->file = REF(file);
    entry->expiry = timeout == CLOCKS_NEVER ? CLOCKS_NEVER : sys_time_uptime() + timeout;

    RWLOCK_WRITE_SCOPE(&keyLock);
    map_key_t mapKey = map_key_buffer(&entry->key, sizeof(entry->key));
    if (map_insert(&keyMap, &mapKey, &entry->mapEntry) == ERR)
    {
        DEREF(entry->file);
        free(entry);
        return ERR;
    }

    *key = entry->key;
    if (list_length(&keyList) == 0)
    {
        list_push_back(&keyList, &entry->entry);
        return 0;
    }

    key_entry_t* other;
    LIST_FOR_EACH(other, &keyList, entry)
    {
        if (entry->expiry < other->expiry)
        {
            list_prepend(&keyList, &other->entry, &entry->entry);
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
    DEREF_DEFER(file);

    key_t keyCopy;
    if (key_share(&keyCopy, file, timeout) == ERR)
    {
        return ERR;
    }

    if (thread_copy_to_user(thread, key, &keyCopy, sizeof(keyCopy)) == ERR)
    {
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

    RWLOCK_WRITE_SCOPE(&keyLock);
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
    DEREF_DEFER(file);

    return file_table_alloc(&process->fileTable, file);
}
