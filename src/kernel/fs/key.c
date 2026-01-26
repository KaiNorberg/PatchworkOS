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

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/list.h>
#include <time.h>

static bool key_entry_cmp(map_entry_t* entry, const void* key)
{
    key_entry_t* e = CONTAINER_OF(entry, key_entry_t, mapEntry);
    return strcmp(e->key, key) == 0;
}

static MAP_CREATE(keyMap, 1024, key_entry_cmp);
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

static status_t key_generate(char* buffer, uint64_t size)
{
    uint64_t hash;
    do
    {
        assert(size <= KEY_MAX);
        uint8_t bytes[((size - 1) / 4) * 3];
        status_t status = rand_gen(bytes, sizeof(bytes));
        if (IS_ERR(status))
        {
            return status;
        }
        key_base64_encode(bytes, sizeof(bytes), buffer);
        hash = hash_buffer(buffer, strlen(buffer));
    } while (map_find(&keyMap, buffer, hash) != NULL);
    return OK;
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

        map_remove(&keyMap, &entry->mapEntry, hash_buffer(entry->key, strlen(entry->key)));
        list_remove(&entry->entry);
        UNREF(entry->file);
        free(entry);
    }
}

status_t key_share(char* key, uint64_t size, file_t* file, clock_t timeout)
{
    if (key == NULL || size == 0 || size > KEY_MAX || file == NULL)
    {
        return ERR(VFS, INVAL);
    }

    key_entry_t* entry = malloc(sizeof(key_entry_t));
    if (entry == NULL)
    {
        return ERR(VFS, NOMEM);
    }
    list_entry_init(&entry->entry);
    map_entry_init(&entry->mapEntry);
    entry->file = REF(file);
    entry->expiry = CLOCKS_DEADLINE(timeout, clock_uptime());

    LOCK_SCOPE(&keyLock);

    status_t status = key_generate(entry->key, size);
    if (IS_ERR(status))
    {
        UNREF(entry->file);
        free(entry);
        return status;
    }

    uint64_t hash = hash_buffer(entry->key, size);
    map_insert(&keyMap, &entry->mapEntry, hash);

    memcpy(key, entry->key, size);
    if (list_is_empty(&keyList))
    {
        list_push_back(&keyList, &entry->entry);
        timer_set(clock_uptime(), entry->expiry);
        return OK;
    }

    bool first = true;
    key_entry_t* other;
    LIST_FOR_EACH(other, &keyList, entry)
    {
        if (entry->expiry < other->expiry)
        {
            list_prepend(&other->entry, &entry->entry);
            if (first)
            {
                timer_set(clock_uptime(), entry->expiry);
            }
            return OK;
        }
        first = false;
    }

    list_push_back(&keyList, &entry->entry);
    return OK;
}

status_t key_claim(file_t** out, const char* key)
{
    if (out == NULL || key == NULL)
    {
        return ERR(VFS, INVAL);
    }

    LOCK_SCOPE(&keyLock);
    uint64_t hash = hash_buffer(key, strlen(key));
    key_entry_t* entry = CONTAINER_OF_SAFE(map_find(&keyMap, key, hash), key_entry_t, mapEntry);
    if (entry == NULL)
    {
        return ERR(VFS, NOENT);
    }

    list_remove(&entry->entry);

    file_t* file = entry->file;
    free(entry);
    *out = file;
    return OK;
}

SYSCALL_DEFINE(SYS_SHARE, char* key, uint64_t size, fd_t fd, clock_t timeout)
{
    thread_t* thread = thread_current();
    process_t* process = thread->process;

    file_t* file = file_table_get(&process->files, fd);
    if (file == NULL)
    {
        return ERR(VFS, BADFD);
    }
    UNREF_DEFER(file);

    char keyCopy[KEY_MAX] = {0};
    status_t status = key_share(keyCopy, size, file, timeout);
    if (IS_ERR(status))
    {
        return status;
    }

    status = thread_copy_to_user(thread, key, keyCopy, size);
    if (IS_ERR(status))
    {
        file_t* ignored = NULL;
        key_claim(&ignored, keyCopy);
        if (ignored != NULL)
        {
            UNREF(ignored);
        }
        return status;
    }
    return OK;
}

SYSCALL_DEFINE(SYS_CLAIM, const char* key)
{
    thread_t* thread = thread_current();
    process_t* process = thread->process;

    char keyCopy[KEY_MAX];
    status_t status = thread_copy_from_user_string(thread, keyCopy, key, KEY_MAX);
    if (IS_ERR(status))
    {
        return status;
    }

    file_t* file;
    status = key_claim(&file, keyCopy);
    if (IS_ERR(status))
    {
        return status;
    }
    UNREF_DEFER(file);

    fd_t fd = file_table_open(&process->files, file);
    if (fd == FD_NONE)
    {
        return ERR(VFS, MFILE);
    }
    *_result = fd;
    return OK;
}
