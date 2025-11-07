#include "socket_family.h"

#include "net.h"
#include "socket.h"
#include <kernel/fs/sysfs.h>
#include <kernel/fs/vfs.h>
#include <kernel/log/log.h>

#include <errno.h>
#include <stdlib.h>
#include <sys/list.h>

static list_t families = LIST_CREATE(families);
static lock_t lock = LOCK_CREATE;

static uint64_t socket_factory_read(file_t* file, void* buffer, uint64_t count, uint64_t* offset)
{
    socket_t* sock = file->private;

    uint64_t length = strlen(sock->id);
    return BUFFER_READ(buffer, count, offset, sock->id, length);
}

static uint64_t socket_factory_open(file_t* file)
{
    socket_factory_t* factory = file->inode->private;

    socket_t* sock = socket_new(factory->family, factory->type, file->flags);
    if (sock == NULL)
    {
        return ERR;
    }

    file->private = sock;
    return 0;
}

static void socket_factory_close(file_t* file)
{
    socket_t* sock = file->private;
    if (sock != NULL)
    {
        DEREF(sock);
    }
}

static file_ops_t fileOps = {
    .read = socket_factory_read,
    .open = socket_factory_open,
    .close = socket_factory_close,
};

uint64_t socket_family_register(const socket_family_ops_t* ops, const char* name, socket_type_t supportedTypes)
{
    if (ops == NULL || name == NULL || supportedTypes == 0)
    {
        errno = EINVAL;
        return ERR;
    }

    if (ops->init == NULL || ops->deinit == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    socket_family_t* family = malloc(sizeof(socket_family_t));
    if (family == NULL)
    {
        return ERR;
    }
    list_entry_init(&family->entry);
    strncpy(family->name, name, MAX_NAME - 1);
    family->name[MAX_NAME - 1] = '\0';
    family->ops = ops;
    family->supportedTypes = supportedTypes;
    atomic_init(&family->newId, 0);
    list_init(&family->factories);

    mount_t* mount = net_get_mount();
    if (mount == NULL)
    {
        free(family);
        return ERR;
    }

    family->dir = sysfs_dir_new(mount->root, family->name, NULL, family);
    DEREF(mount);
    if (family->dir == NULL)
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

        socket_factory_t* factory = malloc(sizeof(socket_factory_t));
        if (factory == NULL)
        {
            goto error;
        }
        list_entry_init(&factory->entry);
        factory->type = type;
        factory->family = family;

        const char* string = socket_type_to_string(type);
        factory->file = sysfs_file_new(family->dir, string, NULL, &fileOps, factory);
        if (factory->file == NULL)
        {
            free(factory);
            goto error;
        }

        list_push_back(&family->factories, &factory->entry);
    }

    lock_acquire(&lock);
    list_push_back(&families, &family->entry);
    lock_release(&lock);

    LOG_INFO("registered family %s\n", family->name);
    return 0;

error:;

    socket_factory_t* temp;
    socket_factory_t* factory;
    LIST_FOR_EACH_SAFE(factory, temp, &family->factories, entry)
    {
        DEREF(factory->file);
        free(factory);
    }

    DEREF(family->dir);
    return ERR;
}

socket_family_t* socket_family_get(const char* name)
{
    LOCK_SCOPE(&lock);
    socket_family_t* family;
    LIST_FOR_EACH(family, &families, entry)
    {
        if (strcmp(family->name, name) == 0)
        {
            return family;
        }
    }
    return NULL;
}

static socket_family_t* socket_family_get_and_remove(const char* name)
{
    LOCK_SCOPE(&lock);
    socket_family_t* family;
    LIST_FOR_EACH(family, &families, entry)
    {
        if (strcmp(family->name, name) == 0)
        {
            list_remove(&families, &family->entry);
            return family;
        }
    }
    return NULL;
}

void socket_family_unregister(const char* name)
{
    socket_family_t* family = socket_family_get_and_remove(name);
    if (family == NULL)
    {
        LOG_WARN("socket family %s not found for unregistration\n", name);
        return;
    }

    socket_factory_t* temp;
    socket_factory_t* factory;
    LIST_FOR_EACH_SAFE(factory, temp, &family->factories, entry)
    {
        DEREF(factory->file);
        free(factory);
    }

    DEREF(family->dir);
    free(family);
    LOG_INFO("unregistered family %s\n", family->name);
    return;
}

void socket_family_unregister_all(void)
{
    socket_family_t* temp;
    socket_family_t* family;
    LIST_FOR_EACH_SAFE(family, temp, &families, entry)
    {
        socket_family_unregister(family->name);
    }
}

uint64_t socket_family_get_dir(socket_family_t* family, path_t* outPath)
{
    if (family == NULL || outPath == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    mount_t* mount = net_get_mount();
    if (mount == NULL)
    {
        return ERR;
    }

    path_set(outPath, mount, family->dir);
    DEREF(mount);

    return 0;
}
