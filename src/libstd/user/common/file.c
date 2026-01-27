#include "file.h"

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/fs.h>

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

int _file_init(FILE* stream, fd_t fd, _file_flags_t flags, void* buffer, size_t size)
{
    if (buffer == NULL)
    {
        void* oldBuf = stream->buf;
        stream->buf = malloc(size);
        if (stream->buf == NULL)
        {
            return EOF;
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
    stream->bufSize = size;
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
        return EOF;
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

int _file_flush_buffer(FILE* stream)
{
    if (!(stream->flags & _FILE_BIN))
    {
        /// @todo Text stream conversion here
    }

    size_t count;
    status_t status = write(stream->fd, stream->buf, stream->bufIndex, &count);
    if (IS_ERR(status))
    {
        stream->flags |= _FILE_ERROR;
        return EOF;
    }

    stream->pos.offset += count;
    stream->bufIndex = 0;
    return 0;
}

int _file_fill_buffer(FILE* stream)
{
    uint64_t count;
    status_t status = read(stream->fd, stream->buf, stream->bufSize, &count);
    if (IS_ERR(status))
    {
        stream->flags |= _FILE_ERROR;
        return EOF;
    }
    if (count == 0)
    {
        stream->flags |= _FILE_EOF;
        return EOF;
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

int _file_seek(FILE* stream, int64_t offset, int whence)
{
    if (whence != SEEK_SET && whence != SEEK_CUR && whence != SEEK_END)
    {
        errno = EINVAL;
        return EOF;
    }

    size_t newPos;
    status_t status = seek(stream->fd, offset, whence, &newPos);
    if (IS_ERR(status))
    {
        return EOF;
    }

    stream->ungetIndex = 0;
    stream->bufIndex = 0;
    stream->bufEnd = 0;
    stream->pos.offset = newPos;
    return 0;
}

int _file_prepare_read(FILE* stream)
{
    if ((stream->bufIndex > stream->bufEnd) ||
        (stream->flags & (_FILE_WRITE | _FILE_APPEND | _FILE_ERROR | _FILE_WIDESTREAM | _FILE_EOF)) ||
        !(stream->flags & (_FILE_READ | _FILE_RW)))
    {
        errno = EBADF;
        stream->flags |= _FILE_ERROR;
        return EOF;
    }

    stream->flags |= _FILE_READ | _FILE_BYTESTREAM;
    return 0;
}

int _file_prepare_write(FILE* stream)
{
    if ((stream->bufIndex < stream->bufEnd) || (stream->ungetIndex > 0) ||
        (stream->flags & (_FILE_READ | _FILE_ERROR | _FILE_WIDESTREAM | _FILE_EOF)) ||
        !(stream->flags & (_FILE_WRITE | _FILE_APPEND | _FILE_RW)))
    {
        errno = EBADF;
        stream->flags |= _FILE_ERROR;
        return EOF;
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
    list_push_back(&files, &file->entry);
    mtx_unlock(&filesMtx);
}

void _files_remove(FILE* file)
{
    mtx_lock(&filesMtx);
    list_remove(&file->entry);
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

int _files_flush(void)
{
    int result = 0;
    mtx_lock(&filesMtx);

    FILE* stream;
    LIST_FOR_EACH(stream, &files, entry)
    {
        if (fflush(stream) == EOF)
        {
            result = EOF;
        }
    }

    mtx_unlock(&filesMtx);

    return result;
}
