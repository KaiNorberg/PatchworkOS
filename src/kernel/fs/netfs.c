#include <kernel/fs/ctl.h>
#include <kernel/fs/dentry.h>
#include <kernel/fs/filesystem.h>
#include <kernel/fs/mount.h>
#include <kernel/fs/path.h>
#include <kernel/fs/devfs.h>
#include <kernel/fs/vfs.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/module/module.h>
#include <kernel/proc/process.h>
#include <kernel/sched/sched.h>
#include <kernel/sync/rwmutex.h>
#include <kernel/fs/netfs.h>

#include <sys/io.h>
#include <sys/list.h>

static list_t families = LIST_CREATE(families);
static rwmutex_t familiesMutex = RWMUTEX_CREATE(familiesMutex);

static void socket_free(socket_t* socket)
{
    if (socket == NULL)
    {
        return;
    }

    rwmutex_write_acquire(&socket->family->mutex);
    if (list_contains_entry(&socket->family->sockets, &socket->listEntry))
    {
        list_remove(&socket->family->sockets, &socket->listEntry);
    }
    rwmutex_write_release(&socket->family->mutex);

    socket->family->deinit(socket);
    free(socket);
}

static socket_t* socket_new(netfs_family_t* family, socket_type_t type)
{
    static _Atomic(uint64_t) nextId = ATOMIC_VAR_INIT(0);

    socket_t* socket = malloc(sizeof(socket_t));
    if (socket == NULL)
    {
        errno = ENOMEM;
        return NULL;
    }

    ref_init(&socket->ref, socket_free);
    list_entry_init(&socket->listEntry);
    snprintf(socket->id, sizeof(socket->id), "%llu", atomic_fetch_add_explicit(&nextId, 1, memory_order_relaxed));
    socket->family = family;
    socket->type = type;
    socket->state = SOCKET_NEW;
    weak_ptr_set(&socket->ownerNs, NULL, NULL, NULL);
    socket->private = NULL;
    mutex_init(&socket->mutex);

    if (socket->family->init(socket) == ERR)
    {
        free(socket);
        return NULL;
    }

    return socket;
}

typedef struct socket_file
{
    const char* name;
    file_ops_t* fileOps;
} socket_file_t;

static uint64_t netfs_data_open(file_t* file)
{
    socket_t* sock = file->inode->private;
    assert(sock != NULL);

    file->private = REF(sock);
    return 0;
}

static void netfs_data_close(file_t* file)
{
    socket_t* sock = file->private;
    if (sock == NULL)
    {
        return;
    }

    UNREF(sock);
}

static uint64_t netfs_data_read(file_t* file, void* buf, size_t count, uint64_t* offset)
{
    socket_t* sock = file->private;
    assert(sock != NULL);

    if (sock->family->recv == NULL)
    {
        errno = ENOSYS;
        return 0;
    }

    MUTEX_SCOPE(&sock->mutex);

    if (sock->state != SOCKET_CONNECTED)
    {
        errno = ENOTCONN;
        return ERR;
    }

    return sock->family->recv(sock, buf, count, offset, file->mode);
}

static uint64_t netfs_data_write(file_t* file, const void* buf, size_t count, uint64_t* offset)
{
    socket_t* sock = file->private;
    assert(sock != NULL);

    if (sock->family->send == NULL)
    {
        errno = ENOSYS;
        return 0;
    }

    MUTEX_SCOPE(&sock->mutex);

    if (sock->state != SOCKET_CONNECTED)
    {
        errno = ENOTCONN;
        return ERR;
    }

    return sock->family->send(sock, buf, count, offset, file->mode);
}

static wait_queue_t* netfs_data_poll(file_t* file, poll_events_t* revents)
{
    socket_t* sock = file->private;
    assert(sock != NULL);

    if (sock->family->poll == NULL)
    {
        errno = ENOSYS;
        return NULL;
    }

    MUTEX_SCOPE(&sock->mutex);

    return sock->family->poll(sock, revents);
}

static file_ops_t dataOps = {
    .open = netfs_data_open,
    .close = netfs_data_close,
    .read = netfs_data_read,
    .write = netfs_data_write,
    .poll = netfs_data_poll,
};

static uint64_t netfs_accept_open(file_t* file)
{
    socket_t* sock = file->inode->private;
    assert(sock != NULL);

    if (sock->family->accept == NULL)
    {
        errno = ENOSYS;
        return ERR;
    }

    MUTEX_SCOPE(&sock->mutex);

    if (sock->state != SOCKET_LISTENING)
    {
        errno = EINVAL;
        return ERR;
    }

    socket_t* newSock = socket_new(sock->family, sock->type);
    if (newSock == NULL)
    {
        return ERR;
    }

    if (sock->family->accept(sock, newSock, file->mode) == ERR)
    {
        socket_free(newSock);
        return ERR;
    }

    newSock->state = SOCKET_CONNECTED;
    file->private = newSock;
    file->ops = &dataOps;
    return 0;
}

static file_ops_t acceptOps = {
    .open = netfs_accept_open,
};

static uint64_t netfs_ctl_bind(file_t* file, uint64_t argc, const char** argv)
{
    UNUSED(argc);

    socket_t* sock = file->inode->private;
    assert(sock != NULL);

    if (sock->family->bind == NULL)
    {
        errno = ENOSYS;
        return ERR;
    }

    MUTEX_SCOPE(&sock->mutex);

    if (sock->state != SOCKET_NEW)
    {
        errno = EINVAL;
        return ERR;
    }

    strncpy(sock->address, argv[1], sizeof(sock->address));
    sock->address[sizeof(sock->address) - 1] = '\0';

    if (sock->family->bind(sock) == ERR)
    {
        return ERR;
    }

    sock->state = SOCKET_BOUND;
    return 0;
}

static uint64_t netfs_ctl_listen(file_t* file, uint64_t argc, const char** argv)
{
    UNUSED(argc);

    uint64_t backlog = NETFS_BACKLOG_DEFAULT;
    if (argc == 2)
    {
        if (sscanf(argv[1], "%llu", &backlog) != 1)
        {
            errno = EINVAL;
            return ERR;
        }

        if (backlog == 0)
        {
            errno = EINVAL;
            return ERR;
        }
    }

    socket_t* sock = file->inode->private;
    assert(sock != NULL);

    if (sock->family->listen == NULL)
    {
        errno = ENOSYS;
        return ERR;
    }

    MUTEX_SCOPE(&sock->mutex);

    if (sock->state != SOCKET_BOUND)
    {
        errno = EINVAL;
        return ERR;
    }

    if (sock->family->listen(sock, backlog) == ERR)
    {
        return ERR;
    }

    sock->state = SOCKET_LISTENING;
    return 0;
}

static uint64_t netfs_ctl_connect(file_t* file, uint64_t argc, const char** argv)
{
    UNUSED(argc);

    socket_t* sock = file->inode->private;
    assert(sock != NULL);

    if (sock->family->connect == NULL)
    {
        errno = ENOSYS;
        return ERR;
    }

    MUTEX_SCOPE(&sock->mutex);

    if (sock->state != SOCKET_NEW && sock->state != SOCKET_BOUND)
    {
        errno = EINVAL;
        return ERR;
    }

    strncpy(sock->address, argv[1], sizeof(sock->address));
    sock->address[sizeof(sock->address) - 1] = '\0';

    if (sock->family->connect(sock) == ERR)
    {
        return ERR;
    }

    sock->state = SOCKET_CONNECTED;
    return 0;
}

CTL_STANDARD_OPS_DEFINE(ctlOps,
    {
        {"bind", netfs_ctl_bind, 2, 2},
        {"listen", netfs_ctl_listen, 1, 2},
        {"connect", netfs_ctl_connect, 2, 2},
        {0},
    });

static socket_file_t socketFiles[] = {
    {.name = "data", .fileOps = &dataOps},
    {.name = "accept", .fileOps = &acceptOps},
    {.name = "ctl", .fileOps = &ctlOps},
};

static uint64_t netfs_socket_lookup(inode_t* dir, dentry_t* dentry)
{
    for (size_t i = 0; i < ARRAY_SIZE(socketFiles); i++)
    {
        if (strcmp(socketFiles[i].name, dentry->name) != 0)
        {
            continue;
        }

        inode_t* inode = inode_new(dir->superblock, ino_gen(dir->number, socketFiles[i].name), INODE_FILE, NULL,
            socketFiles[i].fileOps);
        if (inode == NULL)
        {
            return ERR;
        }
        UNREF_DEFER(inode);
        inode->private = dir->private; // No reference

        dentry_make_positive(dentry, inode);
        return 0;
    }

    return 0;
}

static void netfs_socket_cleanup(inode_t* inode)
{
    socket_t* socket = (socket_t*)inode->private;
    if (socket == NULL)
    {
        return;
    }

    UNREF(socket);
}

static uint64_t netfs_socket_iterate(dentry_t* dentry, dir_ctx_t* ctx)
{
    if (!dentry_iterate_dots(dentry, ctx))
    {
        return 0;
    }

    for (size_t i = 0; i < ARRAY_SIZE(socketFiles); i++)
    {
        if (ctx->index++ < ctx->pos)
        {
            continue;
        }

        if (!ctx->emit(ctx, socketFiles[i].name, ino_gen(dentry->inode->number, socketFiles[i].name), INODE_FILE))
        {
            return 0;
        }
    }

    return 0;
}

static inode_ops_t socketInodeOps = {
    .lookup = netfs_socket_lookup,
    .cleanup = netfs_socket_cleanup,
};

static dentry_ops_t socketDentryOps = {
    .iterate = netfs_socket_iterate,
};

typedef struct netfs_family_file
{
    const char* name;
    socket_type_t type;
    file_ops_t* fileOps;
} netfs_family_file_t;

typedef struct
{
    netfs_family_t* family;
    netfs_family_file_t* fileInfo;
} netfs_family_file_ctx_t;

static void socket_weak_ptr_callback(void* arg)
{
    socket_t* socket = (socket_t*)arg;
    UNREF(socket);
}

static uint64_t netfs_factory_open(file_t* file)
{
    netfs_family_file_ctx_t* ctx = file->inode->private;
    assert(ctx != NULL);
    dentry_t* root = file->inode->superblock->root;
    assert(root != NULL);

    socket_t* socket = socket_new(ctx->family, ctx->fileInfo->type);
    if (socket == NULL)
    {
        return ERR;
    }
    UNREF_DEFER(socket);

    rwmutex_write_acquire(&ctx->family->mutex);
    list_push_back(&ctx->family->sockets, &socket->listEntry);
    rwmutex_write_release(&ctx->family->mutex);

    namespace_t* ns = process_get_ns(sched_process());
    if (ns == NULL)
    {
        return ERR;
    }
    UNREF_DEFER(ns);

    weak_ptr_set(&socket->ownerNs, &ns->ref, socket_weak_ptr_callback, REF(socket));

    file->private = REF(socket);
    return 0;
}

static void netfs_factory_close(file_t* file)
{
    socket_t* socket = file->private;
    if (socket == NULL)
    {
        return;
    }

    UNREF(socket);
}

static uint64_t netfs_factory_read(file_t* file, void* buffer, uint64_t count, uint64_t* offset)
{
    socket_t* socket = file->private;
    if (socket == NULL)
    {
        return 0;
    }

    return BUFFER_READ(buffer, count, offset, socket->id, strlen(socket->id));
}

static file_ops_t factoryFileOps = {
    .open = netfs_factory_open,
    .close = netfs_factory_close,
    .read = netfs_factory_read,
};

static uint64_t netfs_addrs_read(file_t* file, void* buffer, uint64_t count, uint64_t* offset)
{
    netfs_family_file_ctx_t* ctx = file->inode->private;
    assert(ctx != NULL);

    RWMUTEX_READ_SCOPE(&ctx->family->mutex);

    if (list_is_empty(&ctx->family->sockets))
    {
        return 0;
    }

    char* string = malloc(list_length(&ctx->family->sockets) * (MAX_PATH + 1));
    if (string == NULL)
    {
        errno = ENOMEM;
        return ERR;
    }

    uint64_t length = 0;
    socket_t* socket;
    LIST_FOR_EACH(socket, &ctx->family->sockets, listEntry)
    {
        if (socket->family != ctx->family)
        {
            continue;
        }

        if (socket->state != SOCKET_LISTENING)
        {
            continue;
        }

        length += snprintf(string + length, MAX_PATH, "%s\n", socket->address);
    }

    uint64_t bytesRead = BUFFER_READ(buffer, count, offset, string, length);
    free(string);
    return bytesRead;
}

static file_ops_t addrsFileOps = {
    .read = netfs_addrs_read,
};

static netfs_family_file_t familyFiles[] = {
    {.name = "stream", .type = SOCKET_STREAM, .fileOps = &factoryFileOps},
    {.name = "dgram", .type = SOCKET_DGRAM, .fileOps = &factoryFileOps},
    {.name = "seqpacket", .type = SOCKET_SEQPACKET, .fileOps = &factoryFileOps},
    {.name = "raw", .type = SOCKET_RAW, .fileOps = &factoryFileOps},
    {.name = "rdm", .type = SOCKET_RDM, .fileOps = &factoryFileOps},
    {.name = "addrs", .type = 0, .fileOps = &addrsFileOps},
};

static void netfs_file_cleanup(inode_t* inode)
{
    netfs_family_file_ctx_t* ctx = inode->private;
    if (ctx == NULL)
    {
        return;
    }

    free(ctx);
    inode->private = NULL;
}

static inode_ops_t familyFileInodeOps = {
    .cleanup = netfs_file_cleanup,
};

static uint64_t netfs_family_lookup(inode_t* dir, dentry_t* dentry)
{
    netfs_family_t* family = dir->private;
    assert(family != NULL);

    for (size_t i = 0; i < ARRAY_SIZE(familyFiles); i++)
    {
        if (strcmp(familyFiles[i].name, dentry->name) != 0)
        {
            continue;
        }

        inode_t* inode = inode_new(dir->superblock, ino_gen(dir->number, familyFiles[i].name), INODE_FILE,
            &familyFileInodeOps, familyFiles[i].fileOps);
        if (inode == NULL)
        {
            return ERR;
        }
        UNREF_DEFER(inode);

        netfs_family_file_ctx_t* ctx = malloc(sizeof(netfs_family_file_ctx_t));
        if (ctx == NULL)
        {
            return ERR;
        }
        ctx->family = family;
        ctx->fileInfo = &familyFiles[i];
        inode->private = ctx;

        dentry_make_positive(dentry, inode);
        return 0;
    }

    RWMUTEX_READ_SCOPE(&family->mutex);

    if (list_is_empty(&family->sockets))
    {
        return 0;
    }

    namespace_t* ns = process_get_ns(sched_process());
    if (ns == NULL)
    {
        return ERR;
    }
    UNREF_DEFER(ns);

    socket_t* socket;
    LIST_FOR_EACH(socket, &family->sockets, listEntry)
    {
        if (strcmp(socket->id, dentry->name) != 0)
        {
            continue;
        }

        namespace_t* ownerNs = weak_ptr_get(&socket->ownerNs);
        if (ownerNs == NULL)
        {
            continue;
        }
        UNREF_DEFER(ownerNs);

        if (ownerNs != ns)
        {
            continue;
        }

        inode_t* inode = inode_new(dir->superblock, ino_gen(dir->number, socket->id), INODE_DIR, &socketInodeOps, NULL);
        if (inode == NULL)
        {
            return ERR;
        }
        UNREF_DEFER(inode);
        inode->private = REF(socket);

        dentry->ops = &socketDentryOps;

        dentry_make_positive(dentry, inode);
        return 0;
    }

    return 0;
}

static uint64_t netfs_family_iterate(dentry_t* dentry, dir_ctx_t* ctx)
{
    netfs_family_t* family = dentry->inode->private;
    assert(family != NULL);

    if (!dentry_iterate_dots(dentry, ctx))
    {
        return 0;
    }

    for (size_t i = 0; i < ARRAY_SIZE(familyFiles); i++)
    {
        if (ctx->index++ < ctx->pos)
        {
            continue;
        }

        if (!ctx->emit(ctx, familyFiles[i].name, ino_gen(dentry->inode->number, familyFiles[i].name), INODE_FILE))
        {
            return 0;
        }
    }

    RWMUTEX_READ_SCOPE(&family->mutex);

    if (list_is_empty(&family->sockets))
    {
        return 0;
    }

    namespace_t* ns = process_get_ns(sched_process());
    if (ns == NULL)
    {
        return ERR;
    }
    UNREF_DEFER(ns);

    socket_t* socket;
    LIST_FOR_EACH(socket, &family->sockets, listEntry)
    {
        namespace_t* ownerNs = weak_ptr_get(&socket->ownerNs);
        if (ownerNs == NULL)
        {
            continue;
        }
        UNREF_DEFER(ownerNs);

        if (ownerNs != ns)
        {
            continue;
        }

        if (ctx->index++ < ctx->pos)
        {
            continue;
        }

        if (!ctx->emit(ctx, socket->id, ino_gen(dentry->inode->number, socket->id), INODE_DIR))
        {
            return 0;
        }
    }

    return 0;
}

static inode_ops_t familyInodeOps = {
    .lookup = netfs_family_lookup,
};

static dentry_ops_t familyDentryOps = {
    .iterate = netfs_family_iterate,
};

static uint64_t netfs_lookup(inode_t* dir, dentry_t* dentry)
{
    RWMUTEX_READ_SCOPE(&familiesMutex);

    netfs_family_t* family;
    LIST_FOR_EACH(family, &families, listEntry)
    {
        if (strcmp(family->name, dentry->name) != 0)
        {
            continue;
        }

        inode_t* inode =
            inode_new(dir->superblock, ino_gen(dir->number, family->name), INODE_DIR, &familyInodeOps, NULL);
        if (inode == NULL)
        {
            return ERR;
        }
        UNREF_DEFER(inode);
        inode->private = family;

        dentry->ops = &familyDentryOps;

        dentry_make_positive(dentry, inode);
        return 0;
    }

    return 0;
}

static uint64_t netfs_iterate(dentry_t* dentry, dir_ctx_t* ctx)
{
    if (!dentry_iterate_dots(dentry, ctx))
    {
        return 0;
    }

    RWMUTEX_READ_SCOPE(&familiesMutex);

    netfs_family_t* family;
    LIST_FOR_EACH(family, &families, listEntry)
    {
        if (ctx->index++ < ctx->pos)
        {
            continue;
        }

        if (!ctx->emit(ctx, family->name, ino_gen(dentry->inode->number, family->name), INODE_DIR))
        {
            return 0;
        }
    }

    return 0;
}

static inode_ops_t netInodeOps = {
    .lookup = netfs_lookup,
};

static dentry_ops_t netDentryOps = {
    .iterate = netfs_iterate,
};

static dentry_t* netfs_mount(filesystem_t* fs, dev_t device, void* private)
{
    UNUSED(private);

    superblock_t* superblock = superblock_new(fs, device, NULL, NULL);
    if (superblock == NULL)
    {
        return NULL;
    }
    UNREF_DEFER(superblock);

    inode_t* inode = inode_new(superblock, 0, INODE_DIR, &netInodeOps, NULL);
    if (inode == NULL)
    {
        return NULL;
    }
    UNREF_DEFER(inode);

    dentry_t* dentry = dentry_new(superblock, NULL, NULL);
    if (dentry == NULL)
    {
        return NULL;
    }
    dentry->ops = &netDentryOps;

    dentry_make_positive(dentry, inode);

    superblock->root = dentry;
    return superblock->root;
}

static filesystem_t netfs = {
    .name = NETFS_NAME,
    .mount = netfs_mount,
};

void netfs_init(void)
{
    if (filesystem_register(&netfs) == ERR)
    {
        panic(NULL, "Failed to register netfs filesystem");
    }
}

uint64_t netfs_family_register(netfs_family_t* family)
{
    if (family == NULL || family->init == NULL || family->deinit == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    list_entry_init(&family->listEntry);
    list_init(&family->sockets);
    rwmutex_init(&family->mutex);

    rwmutex_write_acquire(&familiesMutex);
    list_push_back(&families, &family->listEntry);
    rwmutex_write_release(&familiesMutex);

    return 0;
}

void netfs_family_unregister(netfs_family_t* family)
{
    rwmutex_write_acquire(&familiesMutex);
    list_remove(&families, &family->listEntry);
    rwmutex_write_release(&familiesMutex);

    rwmutex_deinit(&family->mutex);
}