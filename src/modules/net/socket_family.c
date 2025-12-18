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
static lock_t lock = LOCK_CREATE();

static uint64_t socket_factory_read(file_t* file, void* buffer, uint64_t count, uint64_t* offset)
{
    const char* sockId = file->private;

    uint64_t length = strlen(sockId);
    return BUFFER_READ(buffer, count, offset, sockId, length);
}

static uint64_t socket_factory_open(file_t* file)
{
    socket_factory_t* factory = file->inode->private;

    file->private = malloc(MAX_NAME);
    if (file->private == NULL)
    {
        return ERR;
    }

    if (socket_create(factory->family, factory->type, file->private, MAX_NAME) == ERR)
    {
        free(file->private);
        file->private = NULL;
        return ERR;
    }

    return 0;
}

static void socket_factory_close(file_t* file)
{
    if (file->private != NULL)
    {
        free(file->private);
        file->private = NULL;
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

    family->dir = sysfs_dir_new(mount->source, family->name, NULL, family);
    UNREF(mount);
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
        UNREF(factory->file);
        free(factory);
    }

    UNREF(family->dir);
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
        UNREF(factory->file);
        free(factory);
    }

    UNREF(family->dir);
    free(family);
    LOG_INFO("unregistered family %s\n", family->name);
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

path_t socket_family_get_dir(socket_family_t* family)
{
    if (family == NULL)
    {
        return PATH_EMPTY;
    }

    mount_t* mount = net_get_mount();
    UNREF_DEFER(mount);

    return PATH_CREATE(mount, family->dir);
}
