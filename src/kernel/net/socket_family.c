#include "socket_family.h"

#include "fs/sysfs.h"
#include "fs/vfs.h"
#include "net/socket.h"
#include "mem/heap.h"
#include "log/log.h"
#include "net/net.h"
#include <sys/list.h>

static uint64_t socket_factory_read(file_t* file, void* buffer, uint64_t count, uint64_t* offset)
{
    socket_t* sock = file->private;

    uint64_t length = strlen(sock->id);
    return BUFFER_READ(buffer, count, offset, sock->id, length); // Include null terminator
}

static uint64_t socket_factory_open(file_t* file)
{
    socket_factory_t* factory = file->inode->private;

    socket_t* socket = socket_new(factory->family, factory->type, file->flags);
    if (socket == NULL)
    {
        return NULL;
    }

    file->private = socket;
    return 0;
}

static void socket_factory_cleanup(file_t* file)
{
    socket_t* socket = file->private;
    if (socket != NULL)
    {
        socket_free(socket);
    }
}

static file_ops_t factoryOps = {
    .read = socket_factory_read,
    .open = socket_factory_open,
    .cleanup = socket_factory_cleanup,
};

uint64_t socket_family_register(socket_family_t* family)
{
    if (family == NULL || family->name == NULL) {
        errno = EINVAL;
        return ERR;
    }

    if (family->init == NULL || family->deinit == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    atomic_init(&family->newId, 0);
    list_init(&family->factories);

    if (sysfs_dir_init(&family->dir, net_get_dir(), family->name, NULL, family) == ERR)
    {
        return ERR;
    }

    for (uint64_t i = 0; i < SOCKET_TYPE_AMOUNT; i++)
    {
        socket_type_t type = (1 << i);
        if (!(type & family->supportedTypes))
        {
            continue;
        }

        socket_factory_t* factory = heap_alloc(sizeof(socket_factory_t), HEAP_NONE);
        if (factory == NULL)
        {
            goto error;
        }
        list_entry_init(&factory->entry);
        factory->type = type;
        factory->family = family;

        const char* string = socket_type_to_string(type);
        if (sysfs_file_init(&factory->file, &family->dir, string, NULL, &factoryOps, family) == ERR)
        {
            heap_free(factory);
            goto error;
        }

        list_push(&family->factories, &factory->entry);
    }

    LOG_INFO("socket: registered family %s\n", family->name);
    return 0;

error:;

    socket_factory_t* temp;
    socket_factory_t* factory;
    LIST_FOR_EACH_SAFE(factory, temp, &family->factories, entry)
    {
        sysfs_file_deinit(&factory->file);
        heap_free(factory);
    }
    list_init(&family->factories); // Reset the list

    sysfs_dir_deinit(&family->dir);
    return ERR;
}

void socket_family_unregister(socket_family_t* family)
{
    sysfs_dir_deinit(&family->dir);
    LOG_INFO("socket: unregistered family %s\n", family->name);
    return;
}
