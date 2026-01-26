#include <kernel/fs/ctl.h>
#include <kernel/fs/dentry.h>
#include <kernel/fs/devfs.h>
#include <kernel/fs/filesystem.h>
#include <kernel/fs/mount.h>
#include <kernel/fs/netfs.h>
#include <kernel/fs/path.h>
#include <kernel/fs/vfs.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/module/module.h>
#include <kernel/proc/process.h>
#include <kernel/sched/sched.h>
#include <kernel/sync/rwmutex.h>

#include <sys/fs.h>
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
    list_remove(&socket->listEntry);
    rwmutex_write_release(&socket->family->mutex);

    socket->family->deinit(socket);
    free(socket);
}

static status_t socket_new(socket_t** out, netfs_family_t* family, socket_type_t type)
{
    static _Atomic(uint64_t) nextId = ATOMIC_VAR_INIT(0);

    socket_t* socket = malloc(sizeof(socket_t));
    if (socket == NULL)
    {
        return ERR(FS, NOMEM);
    }

    ref_init(&socket->ref, socket_free);
    list_entry_init(&socket->listEntry);
    snprintf(socket->id, sizeof(socket->id), "%llu", atomic_fetch_add_explicit(&nextId, 1, memory_order_relaxed));
    socket->family = family;
    socket->type = type;
    socket->state = SOCKET_NEW;
    weak_ptr_set(&socket->ownerNs, NULL, NULL, NULL);
    socket->data = NULL;
    mutex_init(&socket->mutex);

    status_t status = socket->family->init(socket);
    if (IS_ERR(status))
    {
        free(socket);
        return status;
    }

    *out = socket;
    return OK;
}

typedef struct socket_file
{
    const char* name;
    file_ops_t* fileOps;
} socket_file_t;

static status_t netfs_data_open(file_t* file)
{
    socket_t* sock = file->vnode->data;
    assert(sock != NULL);

    file->data = REF(sock);
    return OK;
}

static void netfs_data_close(file_t* file)
{
    socket_t* sock = file->data;
    if (sock == NULL)
    {
        return;
    }

    UNREF(sock);
}

static status_t netfs_data_read(file_t* file, void* buf, size_t count, size_t* offset, size_t* bytesRead)
{
    socket_t* sock = file->data;
    assert(sock != NULL);

    if (sock->family->recv == NULL)
    {
        return ERR(FS, IMPL);
    }

    MUTEX_SCOPE(&sock->mutex);

    if (sock->state != SOCKET_CONNECTED)
    {
        return ERR(FS, BADFD);
    }

    return sock->family->recv(sock, buf, count, offset, bytesRead, file->mode);
}

static status_t netfs_data_write(file_t* file, const void* buf, size_t count, size_t* offset, size_t* bytesWritten)
{
    socket_t* sock = file->data;
    assert(sock != NULL);

    if (sock->family->send == NULL)
    {
        return ERR(FS, IMPL);
    }

    MUTEX_SCOPE(&sock->mutex);

    if (sock->state != SOCKET_CONNECTED)
    {
        return ERR(FS, BADFD);
    }

    return sock->family->send(sock, buf, count, offset, bytesWritten, file->mode);
}

static status_t netfs_data_poll(file_t* file, poll_events_t* revents, wait_queue_t** queue)
{
    socket_t* sock = file->data;
    assert(sock != NULL);

    if (sock->family->poll == NULL)
    {
        return ERR(FS, IMPL);
    }

    MUTEX_SCOPE(&sock->mutex);
    return sock->family->poll(sock, revents, queue);
}

static file_ops_t dataOps = {
    .open = netfs_data_open,
    .close = netfs_data_close,
    .read = netfs_data_read,
    .write = netfs_data_write,
    .poll = netfs_data_poll,
};

static status_t netfs_accept_open(file_t* file)
{
    socket_t* sock = file->vnode->data;
    assert(sock != NULL);

    if (sock->family->accept == NULL)
    {
        return ERR(FS, IMPL);
    }

    MUTEX_SCOPE(&sock->mutex);

    if (sock->state != SOCKET_LISTENING)
    {
        return ERR(FS, INVAL);
    }

    socket_t* newSock; 
    status_t status = socket_new(&newSock, sock->family, sock->type);
    if (IS_ERR(status))
    {
        return status;
    }

    status = sock->family->accept(sock, newSock, file->mode);
    if (IS_ERR(status))
    {
        socket_free(newSock);
        return status;
    }

    newSock->state = SOCKET_CONNECTED;
    file->data = newSock;
    file->ops = &dataOps;
    return OK;
}

static file_ops_t acceptOps = {
    .open = netfs_accept_open,
};

static status_t netfs_ctl_bind(file_t* file, uint64_t argc, const char** argv)
{
    UNUSED(argc);

    socket_t* sock = file->vnode->data;
    assert(sock != NULL);

    if (sock->family->bind == NULL)
    {
        return ERR(FS, IMPL);
    }

    MUTEX_SCOPE(&sock->mutex);

    if (sock->state != SOCKET_NEW)
    {
        return ERR(FS, INVAL);
    }

    strncpy(sock->address, argv[1], sizeof(sock->address));
    sock->address[sizeof(sock->address) - 1] = '\0';

    status_t status = sock->family->bind(sock);
    if (IS_ERR(status))
    {
        return status;
    }

    sock->state = SOCKET_BOUND;
    return OK;
}

static status_t netfs_ctl_listen(file_t* file, uint64_t argc, const char** argv)
{
    UNUSED(argc);

    uint64_t backlog = NETFS_BACKLOG_DEFAULT;
    if (argc == 2)
    {
        if (sscanf(argv[1], "%llu", &backlog) != 1)
        {
            return ERR(FS, INVAL);
        }

        if (backlog == 0)
        {
            return ERR(FS, INVAL);
        }
    }

    socket_t* sock = file->vnode->data;
    assert(sock != NULL);

    if (sock->family->listen == NULL)
    {
        return ERR(FS, IMPL);
    }

    MUTEX_SCOPE(&sock->mutex);

    if (sock->state != SOCKET_BOUND)
    {
        return ERR(FS, INVAL);
    }

    status_t status = sock->family->listen(sock, backlog);
    if (IS_ERR(status))
    {
        return status;
    }

    sock->state = SOCKET_LISTENING;
    return OK;
}

static status_t netfs_ctl_connect(file_t* file, uint64_t argc, const char** argv)
{
    UNUSED(argc);

    socket_t* sock = file->vnode->data;
    assert(sock != NULL);

    if (sock->family->connect == NULL)
    {
        return ERR(FS, IMPL);
    }

    MUTEX_SCOPE(&sock->mutex);

    if (sock->state != SOCKET_NEW && sock->state != SOCKET_BOUND)
    {
        return ERR(FS, INVAL);
    }

    strncpy(sock->address, argv[1], sizeof(sock->address));
    sock->address[sizeof(sock->address) - 1] = '\0';

    status_t status = sock->family->connect(sock);
    if (IS_ERR(status))
    {
        return status;
    }

    sock->state = SOCKET_CONNECTED;
    return OK;
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

static status_t netfs_socket_lookup(vnode_t* dir, dentry_t* dentry)
{
    for (size_t i = 0; i < ARRAY_SIZE(socketFiles); i++)
    {
        if (strcmp(socketFiles[i].name, dentry->name) != 0)
        {
            continue;
        }

        vnode_t* vnode = vnode_new(dir->superblock, VREG, NULL, socketFiles[i].fileOps);
        if (vnode == NULL)
        {
            return ERR(FS, NOMEM);
        }
        UNREF_DEFER(vnode);
        vnode->data = dir->data; // No reference

        dentry_make_positive(dentry, vnode);
        return OK;
    }

    return INFO(FS, NEGATIVE);
}

static void netfs_socket_cleanup(vnode_t* vnode)
{
    socket_t* socket = (socket_t*)vnode->data;
    if (socket == NULL)
    {
        return;
    }

    UNREF(socket);
}

static status_t netfs_socket_iterate(dentry_t* dentry, dir_ctx_t* ctx)
{
    if (!dentry_iterate_dots(dentry, ctx))
    {
        return OK;
    }

    for (size_t i = 0; i < ARRAY_SIZE(socketFiles); i++)
    {
        if (ctx->index++ < ctx->pos)
        {
            continue;
        }

        if (!ctx->emit(ctx, socketFiles[i].name, VREG))
        {
            return OK;
        }
    }

    return OK;
}

static vnode_ops_t socketVnodeOps = {
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

static status_t netfs_factory_open(file_t* file)
{
    netfs_family_file_ctx_t* ctx = file->vnode->data;
    assert(ctx != NULL);

    socket_t* socket;
    status_t status = socket_new(&socket, ctx->family, ctx->fileInfo->type);
    if (IS_ERR(status))
    {
        return status;
    }
    UNREF_DEFER(socket);

    rwmutex_write_acquire(&ctx->family->mutex);
    list_push_back(&ctx->family->sockets, &socket->listEntry);
    rwmutex_write_release(&ctx->family->mutex);

    namespace_t* ns = process_get_ns(process_current());
    UNREF_DEFER(ns);

    weak_ptr_set(&socket->ownerNs, &ns->ref, socket_weak_ptr_callback, REF(socket));

    file->data = REF(socket);
    return OK;
}

static void netfs_factory_close(file_t* file)
{
    socket_t* socket = file->data;
    if (socket == NULL)
    {
        return;
    }

    UNREF(socket);
}

static status_t netfs_factory_read(file_t* file, void* buffer, size_t count, size_t* offset, size_t* bytesRead)
{
    socket_t* socket = file->data;
    if (socket == NULL)
    {
        *bytesRead = 0;
        return OK;
    }

    size_t len = strlen(socket->id);
    return buffer_read(buffer, count, offset, bytesRead, socket->id, len);
}

static file_ops_t factoryFileOps = {
    .open = netfs_factory_open,
    .close = netfs_factory_close,
    .read = netfs_factory_read,
};

static status_t netfs_addrs_read(file_t* file, void* buffer, size_t count, size_t* offset, size_t* bytesRead)
{
    netfs_family_file_ctx_t* ctx = file->vnode->data;
    assert(ctx != NULL);

    RWMUTEX_READ_SCOPE(&ctx->family->mutex);

    if (list_is_empty(&ctx->family->sockets))
    {
        *bytesRead = 0;
        return OK;
    }

    char* string = malloc(list_size(&ctx->family->sockets) * (MAX_PATH + 1));
    if (string == NULL)
    {
        return ERR(FS, NOMEM);
    }

    size_t length = 0;
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

    status_t status = buffer_read(buffer, count, offset, bytesRead, string, length);
    free(string);
    return status;
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

static void netfs_file_cleanup(vnode_t* vnode)
{
    netfs_family_file_ctx_t* ctx = vnode->data;
    if (ctx == NULL)
    {
        return;
    }

    free(ctx);
    vnode->data = NULL;
}

static vnode_ops_t familyFileVnodeOps = {
    .cleanup = netfs_file_cleanup,
};

static status_t netfs_family_lookup(vnode_t* dir, dentry_t* dentry)
{
    netfs_family_t* family = dir->data;
    assert(family != NULL);

    for (size_t i = 0; i < ARRAY_SIZE(familyFiles); i++)
    {
        if (strcmp(familyFiles[i].name, dentry->name) != 0)
        {
            continue;
        }

        vnode_t* vnode = vnode_new(dir->superblock, VREG, &familyFileVnodeOps, familyFiles[i].fileOps);
        if (vnode == NULL)
        {
            return ERR(FS, NOMEM);
        }
        UNREF_DEFER(vnode);

        netfs_family_file_ctx_t* ctx = malloc(sizeof(netfs_family_file_ctx_t));
        if (ctx == NULL)
        {
            return ERR(FS, NOMEM);
        }
        ctx->family = family;
        ctx->fileInfo = &familyFiles[i];
        vnode->data = ctx;

        dentry_make_positive(dentry, vnode);
        return OK;
    }

    RWMUTEX_READ_SCOPE(&family->mutex);

    if (list_is_empty(&family->sockets))
    {
        return INFO(FS, NEGATIVE);
    }

    namespace_t* ns = process_get_ns(process_current());
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

        vnode_t* vnode = vnode_new(dir->superblock, VDIR, &socketVnodeOps, NULL);
        if (vnode == NULL)
        {
            return ERR(FS, NOMEM);
        }
        UNREF_DEFER(vnode);
        vnode->data = REF(socket);

        dentry->ops = &socketDentryOps;

        dentry_make_positive(dentry, vnode);
        return OK;
    }

    return INFO(FS, NEGATIVE);
}

static status_t netfs_family_iterate(dentry_t* dentry, dir_ctx_t* ctx)
{
    netfs_family_t* family = dentry->vnode->data;
    assert(family != NULL);

    if (!dentry_iterate_dots(dentry, ctx))
    {
        return OK;
    }

    for (size_t i = 0; i < ARRAY_SIZE(familyFiles); i++)
    {
        if (ctx->index++ < ctx->pos)
        {
            continue;
        }

        if (!ctx->emit(ctx, familyFiles[i].name, VREG))
        {
            return OK;
        }
    }

    RWMUTEX_READ_SCOPE(&family->mutex);

    if (list_is_empty(&family->sockets))
    {
        return OK;
    }

    namespace_t* ns = process_get_ns(process_current());
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

        if (!ctx->emit(ctx, socket->id, VDIR))
        {
            return OK;
        }
    }

    return OK;
}

static vnode_ops_t familyVnodeOps = {
    .lookup = netfs_family_lookup,
};

static dentry_ops_t familyDentryOps = {
    .iterate = netfs_family_iterate,
};

static status_t netfs_lookup(vnode_t* dir, dentry_t* dentry)
{
    RWMUTEX_READ_SCOPE(&familiesMutex);

    netfs_family_t* family;
    LIST_FOR_EACH(family, &families, listEntry)
    {
        if (strcmp(family->name, dentry->name) != 0)
        {
            continue;
        }

        vnode_t* vnode = vnode_new(dir->superblock, VDIR, &familyVnodeOps, NULL);
        if (vnode == NULL)
        {
            return ERR(FS, NOMEM);
        }
        UNREF_DEFER(vnode);
        vnode->data = family;

        dentry->ops = &familyDentryOps;

        dentry_make_positive(dentry, vnode);
        return OK;
    }

    return INFO(FS, NEGATIVE);
}

static status_t netfs_iterate(dentry_t* dentry, dir_ctx_t* ctx)
{
    if (!dentry_iterate_dots(dentry, ctx))
    {
        return OK;
    }

    RWMUTEX_READ_SCOPE(&familiesMutex);

    netfs_family_t* family;
    LIST_FOR_EACH(family, &families, listEntry)
    {
        if (ctx->index++ < ctx->pos)
        {
            continue;
        }

        if (!ctx->emit(ctx, family->name, VDIR))
        {
            return OK;
        }
    }

    return OK;
}

static vnode_ops_t netVnodeOps = {
    .lookup = netfs_lookup,
};

static dentry_ops_t netDentryOps = {
    .iterate = netfs_iterate,
};

static status_t netfs_mount(filesystem_t* fs, dentry_t** out, const char* options, void* data)
{
    UNUSED(data);

    if (options != NULL)
    {
        return ERR(FS, INVAL);
    }

    superblock_t* superblock = superblock_new(fs, NULL, NULL);
    if (superblock == NULL)
    {
        return ERR(FS, NOMEM);
    }
    UNREF_DEFER(superblock);

    vnode_t* vnode = vnode_new(superblock, VDIR, &netVnodeOps, NULL);
    if (vnode == NULL)
    {
        return ERR(FS, NOMEM);
    }
    UNREF_DEFER(vnode);

    dentry_t* dentry = dentry_new(superblock, NULL, NULL);
    if (dentry == NULL)
    {
        return ERR(FS, NOMEM);
    }
    dentry->ops = &netDentryOps;

    dentry_make_positive(dentry, vnode);

    superblock->root = dentry;
    *out = dentry;
    return OK;
}

static filesystem_t netfs = {
    .name = NETFS_NAME,
    .mount = netfs_mount,
};

void netfs_init(void)
{
    if (IS_ERR(filesystem_register(&netfs)))
    {
        panic(NULL, "Failed to register netfs filesystem");
    }
}

status_t netfs_family_register(netfs_family_t* family)
{
    if (family == NULL || family->init == NULL || family->deinit == NULL)
    {
        return ERR(FS, INVAL);
    }

    list_entry_init(&family->listEntry);
    list_init(&family->sockets);
    rwmutex_init(&family->mutex);

    rwmutex_write_acquire(&familiesMutex);
    list_push_back(&families, &family->listEntry);
    rwmutex_write_release(&familiesMutex);

    return OK;
}

void netfs_family_unregister(netfs_family_t* family)
{
    rwmutex_write_acquire(&familiesMutex);
    list_remove(&family->listEntry);
    rwmutex_write_release(&familiesMutex);

    rwmutex_deinit(&family->mutex);
}