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
} _file_flags_t;

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
    _file_flags_t flags;
    mtx_t mtx;
    char filename[MAX_PATH];
} FILE;

#define _FILE_GETC(stream) \
    (((stream)->ungetIndex == 0) ? (unsigned char)(stream)->buf[(stream)->bufIndex++] \
                                 : (unsigned char)(stream)->ungetBuf[--(stream)->ungetIndex])

#define _FILE_CHECK_AVAIL(fh) (((fh->bufIndex == fh->bufEnd) && (fh->ungetIndex == 0)) ? _file_fill_buffer(fh) : 0)

_file_flags_t _file_flags_parse(const char* mode);

FILE* _file_new(void);

void _file_free(FILE* stream);

uint64_t _file_init(FILE* stream, fd_t fd, _file_flags_t flags, void* buffer, uint64_t bufferSize);

void _file_deinit(FILE* stream);

uint64_t _file_flush_buffer(FILE* stream);

uint64_t _file_fill_buffer(FILE* stream);

uint64_t _file_seek(FILE* stream, int64_t offset, int whence);

uint64_t _file_prepare_read(FILE* stream);

uint64_t _file_prepare_write(FILE* stream);

void _files_init(void);

void _files_push(FILE* file);

void _files_remove(FILE* file);

void _files_close(void);

uint64_t _files_flush(void);
