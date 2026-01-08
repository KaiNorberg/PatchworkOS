#include <kernel/utils/fifo.h>

#include <errno.h>
#include <stdint.h>
#include <string.h>

void fifo_init(fifo_t* fifo, uint8_t* buffer, size_t size)
{
    fifo->buffer = buffer;
    fifo->size = size;
    fifo->head = 0;
    fifo->tail = 0;
}

void fifo_reset(fifo_t* fifo)
{
    fifo->head = 0;
    fifo->tail = 0;
}

size_t fifo_bytes_readable(const fifo_t* fifo)
{
    if (fifo->head >= fifo->tail)
    {
        return fifo->head - fifo->tail;
    }

    return fifo->size - (fifo->tail - fifo->head);
}

size_t fifo_bytes_writeable(const fifo_t* fifo)
{
    if (fifo->tail > fifo->head)
    {
        return fifo->tail - fifo->head - 1;
    }

    return fifo->size - (fifo->head - fifo->tail) - 1;
}

size_t fifo_read(fifo_t* fifo, void* buffer, size_t count)
{
    size_t readable = fifo_bytes_readable(fifo);
    if (readable == 0)
    {
        return 0;
    }

    if (count > readable)
    {
        count = readable;
    }

    size_t firstSize = fifo->size - fifo->tail;
    if (firstSize > count)
    {
        firstSize = count;
    }

    memcpy(buffer, fifo->buffer + fifo->tail, firstSize);
    fifo->tail = (fifo->tail + firstSize) % fifo->size;

    size_t remaining = count - firstSize;
    if (remaining > 0)
    {
        memcpy((uint8_t*)buffer + firstSize, fifo->buffer + fifo->tail, remaining);
        fifo->tail = (fifo->tail + remaining) % fifo->size;
    }

    return count;
}

size_t fifo_write(fifo_t* fifo, const void* buffer, size_t count)
{
    size_t writeable = fifo_bytes_writeable(fifo);
    if (count > writeable)
    {
        count = writeable;
    }

    size_t firstSize = fifo->size - fifo->head;
    if (firstSize > count)
    {
        firstSize = count;
    }

    memcpy(fifo->buffer + fifo->head, buffer, firstSize);
    fifo->head = (fifo->head + firstSize) % fifo->size;

    size_t remaining = count - firstSize;
    if (remaining > 0)
    {
        memcpy(fifo->buffer + fifo->head, (uint8_t*)buffer + firstSize, remaining);
        fifo->head = (fifo->head + remaining) % fifo->size;
    }

    return count;
}

void fifo_advance_head(fifo_t* fifo, size_t count)
{
    fifo->head = (fifo->head + count) % fifo->size;
}

void fifo_advance_tail(fifo_t* fifo, size_t count)
{
    fifo->tail = (fifo->tail + count) % fifo->size;
}
