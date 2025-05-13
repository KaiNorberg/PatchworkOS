#pragma once

#include <stdint.h>
#include <stdio.h>
#include <sys/list.h>
#include <threads.h>

#include "platform/user/platform.h"

typedef enum
{
    _FILE_READ = (1 << 0),
    _FILE_WRITE = (1 << 2),
    _FILE_RW = (1 << 3),
    _FILE_APPEND = (1 << 4),
    _FILE_BIN = (1 << 5),
    _FILE_OWNS_BUFFER = (1 << 6),
    _FILE_FULLY_BUFFERED = (1 << 7),
    _FILE_LINE_BUFFERED = (1 << 8),
    _FILE_UNBUFFERED = (1 << 9),
    _FILE_ERROR = (1 << 10),
    _FILE_WIDESTREAM = (1 << 11),
    _FILE_BYTESTREAM = (1 << 12),
    _FILE_DELETE_ON_CLOSE = (1 << 13),
    _FILE_EOF = (1 << 14)
} _FileFlags_t;

typedef struct fpos
{
    uint64_t offset;
    int status;
} fpos_t;

#define _UNGETC_MAX 64
typedef struct FILE
{
    list_entry_t entry;
    fd_t fd;
    uint8_t* buf;
    uint64_t bufSize;
    uint64_t bufIndex;
    uint64_t bufEnd;
    fpos_t pos;
    unsigned char ungetBuf[_UNGETC_MAX];
    uint64_t ungetIndex;
    _FileFlags_t flags;
    _PlatformMutex_t mtx;
    char filename[MAX_PATH];
} FILE;

#define _FILE_GETC(stream) \
    (((stream)->ungetIndex == 0) ? (unsigned char)(stream)->buf[(stream)->bufIndex++] \
                                 : (unsigned char)(stream)->ungetBuf[--(stream)->ungetIndex])

#define _FILE_CHECK_AVAIL(fh) (((fh->bufIndex == fh->bufEnd) && (fh->ungetIndex == 0)) ? _FileFillBuffer(fh) : 0)

_FileFlags_t _FileFlagsParse(const char* mode);

FILE* _FileNew(void);

void _FileFree(FILE* stream);

uint64_t _FileInit(FILE* stream, fd_t fd, _FileFlags_t flags, void* buffer, uint64_t bufferSize);

void _FileDeinit(FILE* stream);

uint64_t _FileFlushBuffer(FILE* stream);

uint64_t _FileFillBuffer(FILE* stream);

uint64_t _FileSeek(FILE* stream, int64_t offset, int whence);

uint64_t _FilePrepareRead(FILE* stream);

uint64_t _FilePrepareWrite(FILE* stream);

static inline int _FilePutcUnlocked(FILE* stream, int c)
{
    if (_FilePrepareWrite(stream) == ERR)
    {
        return EOF;
    }

    stream->buf[stream->bufIndex++] = (char)c;
    if ((stream->bufIndex == stream->bufSize) || ((stream->flags & _FILE_LINE_BUFFERED) && ((char)c == '\n')) ||
        (stream->flags & _FILE_UNBUFFERED))
    {
        // buffer filled, unbuffered stream, or end-of-line.
        c = (_FileFlushBuffer(stream) != ERR) ? c : EOF;
    }

    return c;
}

static inline int _FileUngetcUnlocked(FILE* stream, int c)
{
    int result;

    if (c == EOF || stream->ungetIndex == _UNGETC_MAX)
    {
        result = -1;
    }
    else
    {
        result = stream->ungetBuf[stream->ungetIndex++] = (unsigned char)c;
    }

    return result;
}

void _FilesInit(void);

void _FilesPush(FILE* file);

void _FilesRemove(FILE* file);

void _FilesClose(void);

uint64_t _FilesFlush(void);
