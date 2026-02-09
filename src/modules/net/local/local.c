#include "local.h"

#include "local_conn.h"
#include "local_listen.h"

#include <kernel/fs/ctl.h>
#include <kernel/fs/filesystem.h>
#include <kernel/fs/netfs.h>
#include <kernel/fs/path.h>
#include <kernel/io/irp.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/module/module.h>
#include <kernel/sched/wait.h>
#include <kernel/sync/lock.h>
#include <kernel/utils/fifo.h>
#include <kernel/utils/ref.h>

#include <stdlib.h>
#include <sys/fs.h>
#include <sys/io.h>
#include <sys/list.h>
#include <sys/status.h>

static status_t local_socket_recv(irp_t* irp);

static local_listen_t* local_socket_get_listen(local_socket_t* data)
{
    if (data->listen == NULL)
    {
        return NULL;
    }

    return REF(data->listen);
}

static local_conn_t* local_socket_get_conn(local_socket_t* data)
{
    if (data->conn == NULL)
    {
        return NULL;
    }

    return REF(data->conn);
}

static status_t local_socket_init(socket_t* sock)
{
    if (sock->type != SOCKET_SEQPACKET)
    {
        return ERR(PROTO, INVAL);
    }

    local_socket_t* data = calloc(1, sizeof(local_socket_t));
    if (data == NULL)
    {
        return ERR(PROTO, NOMEM);
    }
    sock->data = data;
    return OK;
}

static void local_socket_deinit(socket_t* sock)
{
    local_socket_t* data = sock->data;
    if (data == NULL)
    {
        return;
    }

    if (data->listen != NULL)
    {
        local_listen_close(data->listen);
        UNREF(data->listen);
    }

    if (data->conn != NULL)
    {
        local_conn_close(data->conn);
        UNREF(data->conn);
    }

    free(data);
    sock->data = NULL;
}

static status_t local_socket_bind(socket_t* sock, const char* address)
{
    local_socket_t* data = sock->data;
    if (data == NULL)
    {
        return ERR(PROTO, INVAL);
    }

    if (data->listen != NULL)
    {
        return ERR(PROTO, INVAL);
    }

    if (data->conn != NULL)
    {
        return ERR(PROTO, INVAL);
    }

    local_listen_t* listen;
    status_t status = local_listen_new(address, &listen);
    if (IS_ERR(status))
    {
        return status;
    }

    data->listen = listen;
    return OK;
}

static status_t local_socket_listen(socket_t* sock, const char* backlog)
{
    local_socket_t* data = sock->data;
    if (data == NULL)
    {
        return ERR(PROTO, INVAL);
    }

    uint32_t backlogInt;
    if (sscanf(backlog, "%u", &backlogInt) != 1)
    {
        backlogInt = NETFS_BACKLOG_DEFAULT;
    }

    local_listen_t* listen = data->listen;
    if (listen == NULL)
    {
        return ERR(PROTO, INVAL);
    }
    LOCK_SCOPE(&listen->lock);

    if (backlogInt < LOCAL_MAX_BACKLOG)
    {
        listen->maxBacklog = backlogInt;
    }

    listen->isClosed = false;
    return OK;
}

static status_t local_socket_connect(socket_t* sock, const char* address)
{
    local_socket_t* data = sock->data;
    if (data == NULL)
    {
        return ERR(PROTO, INVAL);
    }

    if (data->conn != NULL)
    {
        return ERR(PROTO, ALREADY_INIT);
    }

    if (data->listen != NULL)
    {
        return ERR(PROTO, INVAL);
    }

    local_listen_t* listen;
    status_t status = local_listen_find(address, &listen);
    if (IS_ERR(status))
    {
        return status;
    }
    UNREF_DEFER(listen);

    local_conn_t* conn = local_conn_new(listen);
    if (conn == NULL)
    {
        return ERR(PROTO, NOMEM);
    }
    UNREF_DEFER(conn);

    LOCK_SCOPE(&listen->lock);

    if (listen->isClosed)
    {
        return ERR(PROTO, NOENT);
    }

    if (listen->pendingAmount >= listen->maxBacklog)
    {
        return ERR(PROTO, BUSY);
    }

    listen->pendingAmount++;
    list_push_back(&listen->backlog, &conn->entry);

    wait_unblock(&listen->waitQueue, WAIT_ALL, OK);

    irp_t* irp;
    list_t pending = LIST_CREATE(pending);
    irp_claim_list(&pending, &listen->polls);
    while (!list_is_empty(&pending))
    {
        irp = CONTAINER_OF(list_pop_front(&pending), irp_t, entry);
        irp->result = IOPOLL_READ;
        irp_complete(irp, OK);
    }

    data->conn = REF(conn);
    data->isServer = false;
    return OK;
}

static status_t local_socket_accept(socket_t* sock, socket_t* newSock, mode_t mode)
{
    UNUSED(mode);

    local_socket_t* data = sock->data;
    if (data == NULL)
    {
        return ERR(PROTO, INVAL);
    }

    local_listen_t* listen = local_socket_get_listen(data);
    if (listen == NULL)
    {
        return ERR(PROTO, INVAL);
    }
    UNREF_DEFER(listen);

    local_conn_t* conn = NULL;
    while (true)
    {
        LOCK_SCOPE(&listen->lock);

        if (listen->isClosed)
        {
            return ERR(PROTO, CANCELLED);
        }

        if (!list_is_empty(&listen->backlog))
        {
            list_entry_t* entry = list_pop_front(&listen->backlog);
            local_conn_t* container = CONTAINER_OF(entry, local_conn_t, entry);
            conn = REF(container);
            listen->pendingAmount--;
            break;
        }

        status_t status =
            WAIT_BLOCK_LOCK(&listen->waitQueue, &listen->lock, listen->isClosed || !list_is_empty(&listen->backlog));
        if (IS_ERR(status))
        {
            return status;
        }
    }
    UNREF_DEFER(conn);

    assert(conn != NULL);

    local_socket_t* newData = newSock->data;
    if (newData == NULL)
    {
        return ERR(PROTO, INVAL);
    }
    newData->conn = REF(conn);
    newData->isServer = true;

    return OK;
}

static status_t local_conn_cancel(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    socket_t* sock = SOCKET_FROM_IRP(irp);
    local_socket_t* data = sock->data;
    local_conn_t* conn = data->conn;

    lock_acquire(&conn->lock);
    if (list_contains(&irp->entry))
    {
        list_remove(&irp->entry);
    }
    lock_release(&conn->lock);

    return OK;
}

static status_t local_socket_send(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    socket_t* sock = SOCKET_FROM_IRP(irp);

    local_socket_t* data = sock->data;
    if (data == NULL)
    {
        return ERR(PROTO, INVAL);
    }

    local_conn_t* conn = local_socket_get_conn(data);
    if (conn == NULL)
    {
        return ERR(PROTO, NOT_INIT);
    }
    UNREF_DEFER(conn);
    LOCK_SCOPE(&conn->lock);

    if (conn->isClosed)
    {
        return ERR(PROTO, IO);
    }

    size_t count = mdl_size(frame->write.buffer);
    if (count > LOCAL_MAX_PACKET_SIZE)
    {
        return ERR(PROTO, TOOBIG);
    }

    fifo_t* ring = data->isServer ? &conn->s2cFifo : &conn->c2sFifo;
    list_t* writers = data->isServer ? &conn->s2cWriters : &conn->c2sWriters;
    list_t* readers = data->isServer ? &conn->s2cReaders : &conn->c2sReaders;

    local_packet_header_t header = {.magic = LOCAL_PACKET_MAGIC, .size = count};

    size_t totalSize = sizeof(local_packet_header_t) + count;
    if (fifo_bytes_writeable(ring) < totalSize)
    {
        return irp_delay(irp, writers, local_conn_cancel);
    }

    fifo_write(ring, &header, sizeof(local_packet_header_t), NULL);
    fifo_write_mdl(ring, frame->write.buffer, 0, NULL);
    irp->result = count;

    irp_t* reader;
    list_t pending = LIST_CREATE(pending);
    irp_claim_list(&pending, readers);
    while (!list_is_empty(&pending))
    {
        reader = CONTAINER_OF(list_pop_front(&pending), irp_t, entry);
        lock_release(&conn->lock);
        status_t status = local_socket_recv(reader);
        if (!IS_INFO(status) || (!IS_CODE(status, PENDING) && !IS_CODE(status, COMPLETE)))
        {
            irp_complete(reader, status);
        }
        lock_acquire(&conn->lock);
    }

    irp_claim_list(&pending, &conn->polls);
    while (!list_is_empty(&pending))
    {
        irp_t* poll = CONTAINER_OF(list_pop_front(&pending), irp_t, entry);
        irp_frame_t* pollFrame = irp_current(poll);
        poll->result = 0;

        fifo_t* readRing = data->isServer ? &conn->c2sFifo : &conn->s2cFifo;
        fifo_t* writeRing = data->isServer ? &conn->s2cFifo : &conn->c2sFifo;

        if (fifo_bytes_readable(readRing) >= sizeof(local_packet_header_t))
        {
            poll->result |= IOPOLL_READ;
        }
        if (fifo_bytes_writeable(writeRing) >= sizeof(local_packet_header_t) + 1)
        {
            poll->result |= IOPOLL_WRITE;
        }

        if (poll->result & pollFrame->poll.events)
        {
            irp_complete(poll, OK);
        }
        else
        {
            irp_delay(poll, &conn->polls, local_conn_cancel);
        }
    }

    return OK;
}

static status_t local_socket_recv(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    socket_t* sock = SOCKET_FROM_IRP(irp);

    local_socket_t* data = sock->data;
    if (data == NULL)
    {
        return ERR(PROTO, INVAL);
    }

    local_conn_t* conn = local_socket_get_conn(data);
    if (conn == NULL)
    {
        return ERR(PROTO, NOT_INIT);
    }
    UNREF_DEFER(conn);
    LOCK_SCOPE(&conn->lock);

    fifo_t* ring = data->isServer ? &conn->c2sFifo : &conn->s2cFifo;
    list_t* writers = data->isServer ? &conn->c2sWriters : &conn->s2cWriters;
    list_t* readers = data->isServer ? &conn->c2sReaders : &conn->s2cReaders;

    if (fifo_bytes_readable(ring) < sizeof(local_packet_header_t))
    {
        if (conn->isClosed)
        {
            irp->result = 0;
            return OK;
        }
        return irp_delay(irp, readers, local_conn_cancel);
    }

    local_packet_header_t header;
    fifo_read(ring, &header, sizeof(local_packet_header_t), NULL);

    status_t error = OK;
    if (header.magic != LOCAL_PACKET_MAGIC)
    {
        error = ERR(PROTO, ILSEQ);
    }
    else if (header.size > LOCAL_MAX_PACKET_SIZE)
    {
        error = ERR(PROTO, TOOBIG);
    }

    if (IS_ERR(error))
    {
        conn->isClosed = true;
        list_t pending = LIST_CREATE(pending);
        irp_claim_list(&pending, readers);
        irp_claim_list(&pending, writers);
        irp_claim_list(&pending, &conn->polls);
        while (!list_is_empty(&pending))
        {
            irp_t* item = CONTAINER_OF(list_pop_front(&pending), irp_t, entry);
            irp_complete(item, error);
        }
        return error;
    }

    size_t count = mdl_size(frame->read.buffer);
    size_t readCount = header.size < count ? header.size : count;
    fifo_read_mdl(ring, frame->read.buffer, 0, NULL);

    if (header.size > readCount)
    {
        uint64_t remaining = header.size - readCount;
        fifo_advance_tail(ring, remaining);
    }
    irp->result = readCount;

    irp_t* writer;
    list_t pending = LIST_CREATE(pending);
    irp_claim_list(&pending, writers);
    while (!list_is_empty(&pending))
    {
        writer = CONTAINER_OF(list_pop_front(&pending), irp_t, entry);
        lock_release(&conn->lock);
        status_t status = local_socket_send(writer);
        if (!IS_INFO(status) || (!IS_CODE(status, PENDING) && !IS_CODE(status, COMPLETE)))
        {
            irp_complete(writer, status);
        }
        lock_acquire(&conn->lock);
    }

    irp_claim_list(&pending, &conn->polls);
    while (!list_is_empty(&pending))
    {
        irp_t* poll = CONTAINER_OF(list_pop_front(&pending), irp_t, entry);
        irp_frame_t* pollFrame = irp_current(poll);
        poll->result = 0;

        fifo_t* readRing = data->isServer ? &conn->c2sFifo : &conn->s2cFifo;
        fifo_t* writeRing = data->isServer ? &conn->s2cFifo : &conn->c2sFifo;

        if (fifo_bytes_readable(readRing) >= sizeof(local_packet_header_t))
        {
            poll->result |= IOPOLL_READ;
        }
        if (fifo_bytes_writeable(writeRing) >= sizeof(local_packet_header_t) + 1)
        {
            poll->result |= IOPOLL_WRITE;
        }

        if (poll->result & pollFrame->poll.events)
        {
            irp_complete(poll, OK);
        }
        else
        {
            irp_delay(poll, &conn->polls, local_conn_cancel);
        }
    }

    return OK;
}

static status_t local_listen_cancel(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    socket_t* sock = SOCKET_FROM_IRP(irp);
    local_socket_t* data = sock->data;
    local_listen_t* listen = data->listen;

    lock_acquire(&listen->lock);
    if (list_contains(&irp->entry))
    {
        list_remove(&irp->entry);
    }
    lock_release(&listen->lock);

    return OK;
}

static status_t local_socket_poll(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    socket_t* sock = SOCKET_FROM_IRP(irp);

    local_socket_t* data = sock->data;
    if (data == NULL)
    {
        return ERR(PROTO, INVAL);
    }

    irp->result = 0;

    switch (sock->state)
    {
    case SOCKET_LISTENING:
    {
        local_listen_t* listen = data->listen;
        if (listen == NULL)
        {
            irp->result |= IOPOLL_ERROR;
            return OK;
        }

        LOCK_SCOPE(&listen->lock);
        if (listen->isClosed)
        {
            irp->result |= IOPOLL_ERROR;
        }
        else if (listen->pendingAmount > 0)
        {
            irp->result |= IOPOLL_READ;
        }

        if (irp->result & frame->poll.events)
        {
            return OK;
        }

        return irp_delay(irp, &listen->polls, local_listen_cancel);
    }
    case SOCKET_CONNECTED:
    {
        local_conn_t* conn = data->conn;
        if (conn == NULL)
        {
            irp->result |= IOPOLL_ERROR;
            return OK;
        }

        LOCK_SCOPE(&conn->lock);
        if (conn->isClosed)
        {
            irp->result |= IOPOLL_HUP;
        }
        else
        {
            fifo_t* readRing = data->isServer ? &conn->c2sFifo : &conn->s2cFifo;
            fifo_t* writeRing = data->isServer ? &conn->s2cFifo : &conn->c2sFifo;

            if (fifo_bytes_readable(readRing) >= sizeof(local_packet_header_t))
            {
                irp->result |= IOPOLL_READ;
            }

            if (fifo_bytes_writeable(writeRing) >= sizeof(local_packet_header_t) + 1)
            {
                irp->result |= IOPOLL_WRITE;
            }
        }

        if (irp->result & frame->poll.events)
        {
            return OK;
        }

        return irp_delay(irp, &conn->polls, local_conn_cancel);
    }
    default:
        return ERR(PROTO, INVAL);
    }
}

static status_t local_socket_control(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    socket_t* sock = SOCKET_FROM_IRP(irp);

    switch (frame->control.command)
    {
    case IOCMD('b', 'i', 'n', 'd'):
        return local_socket_bind(sock, frame->control.args);
    case IOCMD('l', 'i', 's', 't', 'e', 'n'):
        return local_socket_listen(sock, frame->control.args);
    case IOCMD('c', 'o', 'n', 'n', 'e', 'c', 't'):
        return local_socket_connect(sock, frame->control.args);
    default:
        return ERR(PROTO, INVAL);
    }
}

static netfs_family_t local = {
    .name = "local",
    .init = local_socket_init,
    .deinit = local_socket_deinit,
    .accept = local_socket_accept,
    .control = local_socket_control,
    .send = local_socket_send,
    .recv = local_socket_recv,
    .poll = local_socket_poll,
};

status_t _module_procedure(const module_event_t* event)
{
    switch (event->type)
    {
    case MODULE_EVENT_LOAD:
    {
        status_t status = netfs_family_register(&local);
        if (IS_ERR(status))
        {
            return status;
        }
    }
    break;
    case MODULE_EVENT_UNLOAD:
        netfs_family_unregister(&local);
        break;
    default:
        break;
    }

    return OK;
}

MODULE_INFO("Local Networking", "Kai Norberg", "Local networking module", OS_VERSION, "MIT", "BOOT_ALWAYS");