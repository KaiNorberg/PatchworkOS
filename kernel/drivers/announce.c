#include <kernel/drivers/announce.h>
#include <kernel/fs/devfs.h>
#include <kernel/fs/stringstream.h>
#include <kernel/log/log.h>
#include <kernel/sched/clock.h>

#include <assert.h>
#include <stdio.h>

static mutex_t mutex = MUTEX_CREATE(mutex);
static dentry_t* announce = NULL;
static stringstream_t stream = STRINGSTREAM_CREATE(stream);

static vnode_class_t announceClass = {
    .name = "announce",
    .type = FILE_TYPE_SYSTEM,
    .handlers =
        {
            VNODE_HANDLERS(),
            STRINGSTREAM_HANDLERS(),
        },
};

void announce_init(void)
{
    announce = devfs_dentry_new(NULL, "announce", &announceClass, &stream);
    if (announce == NULL)
    {
        panic(NULL, "Failed to init announce directory");
    }
}

status_t announce_device(const char* type, const char* compat, const char* name, announce_change_t change)
{
    if (type == NULL || name == NULL)
    {
        return ERR(FS, INVAL);
    }

    MUTEX_SCOPE(&mutex);

    const char* changeString = NULL;
    switch (change)
    {
    case ANNOUNCE_ATTACH:
    {
        changeString = "attach";
    }
    break;
    case ANNOUNCE_DETACH:
    {
        changeString = "detach";
    }
    break;
    default:
        break;
    }

    if (changeString == NULL)
    {
        return ERR(FS, INVAL);
    }

    char buffer[256];
    int length = snprintf(buffer, sizeof(buffer), "%llu %s %s %s %s\n", clock_uptime(), changeString, type,
        compat != NULL ? compat : "-", name);
    if (length < 0 || length >= (int)sizeof(buffer))
    {
        return ERR(FS, INVAL);
    }

    stringstream_broadcast(&stream, buffer, length);

    return OK;
}