#include "file.h"

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/io.h>

static list_t files;
static mtx_t filesMtx;

_file_flags_t _file_flags_parse(const char* mode)
{
    if (mode == NULL)
    {
        return 0;
    }

    _file_flags_t files = 0;

    switch (mode[0])
    {
    case 'r':
    {
        files |= _FILE_READ;
    }
    break;
    case 'w':
    {
        files |= _FILE_WRITE;
    }
    break;
    case 'a':
    {
        files |= _FILE_APPEND | _FILE_WRITE;
    }
    break;
    default:
    {
        return 0;
    }
    }

    for (uint64_t i = 1; i < 4; i++)
    {
        switch (mode[i])
        {
        case '+':
        {
            if ((files & _FILE_RW) == _FILE_RW)
            {
                return 0;
            }
            files |= _FILE_RW;
        }
        break;
        case 'b':
        {
            if (files & _FILE_BIN)
            {
                return 0;
            }
            files |= _FILE_BIN;
        }
        break;
        case '\0':
        {
            return files;
        }
        default:
        {
            return 0;
        }
        }
    }

    return 0;
}

FILE* _file_new(void)
{
    FILE* stream = calloc(1, sizeof(FILE));
    if (stream == NULL)
    {
        return NULL;
    }

    list_entry_init(&stream->entry);
    return stream;
}

void _file_free(FILE* stream)
{
    if (stream != stdin && stream != stdout && stream != stderr)
    {
        free(stream);
    }
}

uint64_t _file_init(FILE* stream, fd_t fd, _file_flags_t flags, void* buffer, uint64_t bufferSize)
{
    if (buffer == NULL)
    {
        void* oldBuf = stream->buf;
        stream->buf = malloc(bufferSize);
        if (stream->buf == NULL)
        {
            return ERR;
        }
        if (stream->flags & _FILE_OWNS_BUFFER)
        {
            free(oldBuf);
        }
        stream->flags = flags | _FILE_OWNS_BUFFER;
    }
    else
    {
        if (stream->flags & _FILE_OWNS_BUFFER)
        {
            free(stream->buf);
        }
        stream->buf = buffer;
        stream->flags = flags;
    }

    stream->fd = fd;
    stream->bufSize = bufferSize;
    stream->bufIndex = 0;
    stream->bufEnd = 0;
    stream->pos.offset = 0;
    stream->pos.status = 0;
    stream->ungetIndex = 0;

    if (mtx_init(&stream->mtx, mtx_recursive) != thrd_success)
    {
        if (stream->flags & _FILE_OWNS_BUFFER)
        {
            free(stream->buf);
        }
        return ERR;
    }

    return 0;
}

void _file_deinit(FILE* stream)
{
    if (stream->flags & _FILE_OWNS_BUFFER)
    {
        free(stream->buf);
    }

    close(stream->fd);

    mtx_destroy(&stream->mtx);
}

uint64_t _file_flush_buffer(FILE* stream)
{
    if (!(stream->flags & _FILE_BIN))
    {
        // TODO: Text stream conversion here
    }

    uint64_t count = write(stream->fd, stream->buf, stream->bufIndex);
    if (count == ERR)
    {
        stream->flags |= _FILE_ERROR;
        return ERR;
    }

    stream->pos.offset += count;
    stream->bufIndex = 0;
    return 0;
}

uint64_t _file_fill_buffer(FILE* stream)
{
    uint64_t count = read(stream->fd, stream->buf, stream->bufSize);
    if (count == ERR)
    {
        stream->flags |= _FILE_ERROR;
        return ERR;
    }
    if (count == 0)
    {
        stream->flags |= _FILE_EOF;
        return ERR;
    }

    if (!(stream->flags & _FILE_BIN))
    {
        /* TODO: Text stream conversion here */
    }

    stream->pos.offset += count;
    stream->bufEnd = count;
    stream->bufIndex = 0;
    return 0;
}

uint64_t _file_seek(FILE* stream, int64_t offset, int whence)
{
    if (whence != SEEK_SET && whence != SEEK_CUR && whence != SEEK_END)
    {
        errno = EINVAL;
        return ERR;
    }

    uint64_t result = seek(stream->fd, offset, whence);

    if (result == ERR)
    {
        return ERR;
    }

    stream->ungetIndex = 0;
    stream->bufIndex = 0;
    stream->bufEnd = 0;
    stream->pos.offset = result;
    return result;
}

uint64_t _file_prepare_read(FILE* stream)
{
    if ((stream->bufIndex > stream->bufEnd) ||
        (stream->flags & (_FILE_WRITE | _FILE_APPEND | _FILE_ERROR | _FILE_WIDESTREAM | _FILE_EOF)) ||
        !(stream->flags & (_FILE_READ | _FILE_RW)))
    {
        errno = EBADF;
        stream->flags |= _FILE_ERROR;
        return ERR;
    }

    stream->flags |= _FILE_READ | _FILE_BYTESTREAM;
    return 0;
}

uint64_t _file_prepare_write(FILE* stream)
{
    if ((stream->bufIndex < stream->bufEnd) || (stream->ungetIndex > 0) ||
        (stream->flags & (_FILE_READ | _FILE_ERROR | _FILE_WIDESTREAM | _FILE_EOF)) ||
        !(stream->flags & (_FILE_WRITE | _FILE_APPEND | _FILE_RW)))
    {
        errno = EBADF;
        stream->flags |= _FILE_ERROR;
        return ERR;
    }

    stream->flags |= _FILE_WRITE | _FILE_BYTESTREAM;
    return 0;
}

void _files_init(void)
{
    list_init(&files);
    if (mtx_init(&filesMtx, mtx_recursive) != thrd_success)
    {
        fprintf(stderr, "libstd: failed to initialize files mutex\n");
        abort();
    }
}

void _files_push(FILE* file)
{
    mtx_lock(&filesMtx);
    list_push(&files, &file->entry);
    mtx_unlock(&filesMtx);
}

void _files_remove(FILE* file)
{
    mtx_lock(&filesMtx);
    list_remove(&files, &file->entry);
    mtx_unlock(&filesMtx);
}

void _files_close(void)
{
    mtx_lock(&filesMtx);

    FILE* temp;
    FILE* stream;
    LIST_FOR_EACH_SAFE(stream, temp, &files, entry)
    {
        fclose(stream);
    }

    mtx_unlock(&filesMtx);
}

uint64_t _files_flush(void)
{
    uint64_t result = 0;
    mtx_lock(&filesMtx);

    FILE* stream;
    LIST_FOR_EACH(stream, &files, entry)
    {
        if (fflush(stream) == EOF)
        {
            result = ERR;
        }
    }

    mtx_unlock(&filesMtx);

    return result;
}
