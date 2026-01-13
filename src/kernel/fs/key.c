#include <kernel/fs/key.h>

#include <kernel/cpu/syscall.h>
#include <kernel/drivers/rand.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/proc/process.h>
#include <kernel/sched/clock.h>
#include <kernel/sched/thread.h>
#include <kernel/sched/timer.h>
#include <kernel/sched/wait.h>
#include <kernel/sync/rwlock.h>
#include <kernel/utils/map.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/list.h>
#include <time.h>

static map_t keyMap = MAP_CREATE();
static list_t keyList = LIST_CREATE(keyList);
static lock_t keyLock = LOCK_CREATE();

static void key_base64_encode(const uint8_t* src, size_t len, char* dest)
{
    static const char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

    for (size_t i = 0; i < len;)
    {
        uint32_t octetA = i < len ? src[i++] : 0;
        uint32_t octetB = i < len ? src[i++] : 0;
        uint32_t octetC = i < len ? src[i++] : 0;

        uint32_t n = (octetA << 16) | (octetB << 8) | octetC;
        *dest++ = table[(n >> 18) & 0x3F];
        *dest++ = table[(n >> 12) & 0x3F];
        *dest++ = table[(n >> 6) & 0x3F];
        *dest++ = table[n & 0x3F];
    }

    *dest = '\0';
}

static uint64_t key_generate(char* buffer, uint64_t size)
{
    map_key_t mapKey;
    do
    {
        assert(size <= KEY_MAX);
        uint8_t bytes[((size - 1) / 4) * 3];
        if (rand_gen(bytes, sizeof(bytes)) == ERR)
        {
            return ERR;
        }
        key_base64_encode(bytes, sizeof(bytes), buffer);
        mapKey = map_key_string(buffer);
    } while (map_get(&keyMap, &mapKey) != NULL);
    return 0;
}

static void key_timer_handler(interrupt_frame_t* frame, cpu_t* self)
{
    UNUSED(frame);
    UNUSED(self);

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
        list_remove(&entry->entry);
        UNREF(entry->file);
        free(entry);
    }
}

uint64_t key_share(char* key, uint64_t size, file_t* file, clock_t timeout)
{
    if (key == NULL || size == 0 || size > KEY_MAX || file == NULL)
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

    if (key_generate(entry->key, size) == ERR)
    {
        UNREF(entry->file);
        free(entry);
        return ERR;
    }

    map_key_t mapKey = map_key_string(entry->key);
    if (map_insert(&keyMap, &mapKey, &entry->mapEntry) == ERR)
    {
        UNREF(entry->file);
        free(entry);
        return ERR;
    }

    memcpy(key, entry->key, size);
    if (list_is_empty(&keyList))
    {
        list_push_back(&keyList, &entry->entry);
        timer_set(clock_uptime(), entry->expiry);
        return 0;
    }

    bool first = true;
    key_entry_t* other;
    LIST_FOR_EACH(other, &keyList, entry)
    {
        if (entry->expiry < other->expiry)
        {
            list_prepend(&other->entry, &entry->entry);
            if (first)
                timer_set(clock_uptime(), entry->expiry);
            return 0;
        }
        first = false;
    }

    list_push_back(&keyList, &entry->entry);
    return 0;
}

file_t* key_claim(const char* key)
{
    if (key == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    LOCK_SCOPE(&keyLock);
    map_key_t mapKey = map_key_string(key);
    map_entry_t* mapEntry = map_get_and_remove(&keyMap, &mapKey);
    if (mapEntry == NULL)
    {
        errno = ENOENT;
        return NULL;
    }

    key_entry_t* entry = CONTAINER_OF(mapEntry, key_entry_t, mapEntry);
    list_remove(&entry->entry);

    file_t* file = entry->file;
    free(entry);
    return file;
}

SYSCALL_DEFINE(SYS_SHARE, uint64_t, char* key, uint64_t size, fd_t fd, clock_t timeout)
{
    thread_t* thread = thread_current();
    process_t* process = thread->process;

    file_t* file = file_table_get(&process->fileTable, fd);
    if (file == NULL)
    {
        return ERR;
    }
    UNREF_DEFER(file);

    char keyCopy[KEY_MAX] = {0};
    if (key_share(keyCopy, size, file, timeout) == ERR)
    {
        return ERR;
    }

    if (thread_copy_to_user(thread, key, keyCopy, size) == ERR)
    {
        UNREF(key_claim(keyCopy));
        return ERR;
    }
    return 0;
}

SYSCALL_DEFINE(SYS_CLAIM, fd_t, const char* key)
{
    thread_t* thread = thread_current();
    process_t* process = thread->process;

    char keyCopy[KEY_MAX];
    if (thread_copy_from_user_string(thread, keyCopy, key, KEY_MAX) == ERR)
    {
        return ERR;
    }

    file_t* file = key_claim(keyCopy);
    if (file == NULL)
    {
        return ERR;
    }
    UNREF_DEFER(file);

    return file_table_open(&process->fileTable, file);
}
