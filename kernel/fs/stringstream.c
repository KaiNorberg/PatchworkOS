#include <kernel/fs/stringstream.h>

#include <kernel/fs/file.h>
#include <kernel/fs/vnode.h>
#include <kernel/io/irp.h>
#include <kernel/log/panic.h>
#include <stdlib.h>

static cache_t clientCache =
    CACHE_CREATE(clientCache, "stringstream_client", sizeof(stringstream_client_t), CACHE_LINE, NULL, NULL);

static status_t stringstream_cancel(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    stringstream_t* stream = frame->vnode->data;

    lock_acquire(&stream->lock);

    if (list_contains(&irp->entry))
    {
        list_remove(&irp->entry);
    }

    lock_release(&stream->lock);

    return OK;
}

void stringstream_init(stringstream_t* stream)
{
    if (stream == NULL)
    {
        return;
    }

    list_init(&stream->pending);
    list_init(&stream->clients);
    lock_init(&stream->lock);
}

void stringstream_deinit(stringstream_t* stream)
{
    if (stream == NULL)
    {
        return;
    }

    if (!list_is_empty(&stream->pending))
    {
        panic(NULL, "Attempted to free stringstream with pending IRPs");
    }
    if (!list_is_empty(&stream->clients))
    {
        panic(NULL, "Attempted to free stringstream with clients");
    }
}

status_t stringstream_open(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    file_t* file = frame->file;
    if (file == NULL)
    {
        return ERR(IO, EXPECT_FILE);
    }

    stringstream_t* stream = frame->vnode->data;
    assert(stream != NULL);

    stringstream_client_t* client = cache_alloc(&clientCache);
    if (client == NULL)
    {
        return ERR(IO, NOMEM);
    }
    list_entry_init(&client->entry);
    FIFO_DEFINE_INIT(client->fifo);
    client->head = 0;
    client->tail = 0;

    lock_acquire(&stream->lock);
    list_push_back(&stream->clients, &client->entry);
    lock_release(&stream->lock);

    file->data = client;
    return OK;
}

status_t stringstream_close(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    file_t* file = frame->file;
    if (file == NULL)
    {
        return ERR(IO, EXPECT_FILE);
    }

    stringstream_t* stream = frame->vnode->data;
    assert(stream != NULL);

    stringstream_client_t* client = file->data;
    if (client != NULL)
    {
        lock_acquire(&stream->lock);
        list_remove(&client->entry);
        lock_release(&stream->lock);

        cache_free(client);
    }
    return OK;
}

status_t stringstream_read(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    stringstream_t* stream = frame->vnode->data;
    assert(stream != NULL);

    if (frame->file == NULL)
    {
        return ERR(IO, EXPECT_FILE);
    }

    stringstream_client_t* client = frame->file->data;
    assert(client != NULL);

    LOCK_SCOPE(&stream->lock);

    if (client->head == client->tail)
    {
        return irp_delay(irp, &stream->pending, stringstream_cancel);
    }

    size_t cap = sglist_size(frame->read.buffer);
    size_t limit = 0;
    size_t i = client->head;

    while (i != client->tail)
    {
        if (limit + client->lengths[i] > cap)
        {
            break;
        }

        limit += client->lengths[i];
        i = (i + 1) % ARRAY_SIZE(client->lengths);
    }

    if (limit == 0)
    {
        return ERR(IO, INVAL);
    }

    status_t status = fifo_read_sglist(&client->fifo, frame->read.buffer, limit, 0, &irp->result);

    size_t consumed = 0;
    while (client->head != client->tail && consumed < irp->result)
    {
        consumed += client->lengths[client->head];
        client->head = (client->head + 1) % ARRAY_SIZE(client->lengths);
    }

    return status;
}

status_t stringstream_poll(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    stringstream_t* stream = frame->vnode->data;
    assert(stream != NULL);

    if (frame->file == NULL)
    {
        return ERR(IO, EXPECT_FILE);
    }

    stringstream_client_t* client = frame->file->data;
    assert(client != NULL);

    LOCK_SCOPE(&stream->lock);

    if (client->head != client->tail)
    {
        irp->result = IOEVENT_READ;
        return OK;
    }

    return irp_delay(irp, &stream->pending, stringstream_cancel);
}

void stringstream_broadcast(stringstream_t* stream, const char* string, size_t length)
{
    if (stream == NULL || string == NULL || length == 0)
    {
        return;
    }

    list_t pending = LIST_CREATE(pending);

    {
        LOCK_SCOPE(&stream->lock);

        stringstream_client_t* client;
        LIST_FOR_EACH(client, &stream->clients, entry)
        {
            size_t nextTail = (client->tail + 1) % ARRAY_SIZE(client->lengths);
            if (nextTail != client->head && fifo_bytes_writeable(&client->fifo) >= length)
            {
                fifo_write(&client->fifo, string, length, NULL);
                client->lengths[client->tail] = length;
                client->tail = nextTail;
            }
        }
        irp_claim_list(&pending, &stream->pending);
    }

    while (true)
    {
        lock_acquire(&stream->lock);
        if (list_is_empty(&pending))
        {
            lock_release(&stream->lock);
            break;
        }
        irp_t* irp = CONTAINER_OF(list_pop_front(&pending), irp_t, entry);
        lock_release(&stream->lock);

        irp_frame_t* frame = irp_current(irp);
        if (frame->major == IRP_MJ_READ)
        {
            status_t status = stringstream_read(irp);
            if (IS_INFO(status) && IS_CODE(status, PENDING))
            {
                continue;
            }
            irp_complete(irp, status);
        }
        else if (frame->major == IRP_MJ_POLL)
        {
            status_t status = stringstream_poll(irp);
            if (IS_INFO(status) && IS_CODE(status, PENDING))
            {
                continue;
            }
            irp_complete(irp, status);
        }
        else
        {
            irp_complete(irp, ERR(IO, INVAL));
        }
    }
}