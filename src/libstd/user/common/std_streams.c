#include "std_streams.h"
#include "file.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fs.h>

static uint8_t _stdin_buff[BUFSIZ];
static uint8_t _stdout_buff[BUFSIZ];
static uint8_t _stderr_buff[BUFSIZ];

static FILE _stdin;
static FILE _stdout;
static FILE _stderr;

FILE* stdin;
FILE* stdout;
FILE* stderr;

static void _std_stream_init(fd_t fd, FILE* stream, FILE** streamPtr, void* buffer, _file_flags_t flags)
{
    memset(stream, 0, sizeof(FILE));
    list_entry_init(&stream->entry);

    if (_file_init(stream, fd, flags, buffer, BUFSIZ) == ERR)
    {
        fprintf(stderr, "libstd: failed to initialize standard stream (fd=%d)\n", fd);
        abort();
    }

    _files_push(stream);
    *streamPtr = stream;
}

void _std_streams_init(void)
{
    _std_stream_init(STDIN_FILENO, &_stdin, &stdin, _stdin_buff, _FILE_LINE_BUFFERED | _FILE_READ);
    _std_stream_init(STDOUT_FILENO, &_stdout, &stdout, _stdout_buff, _FILE_LINE_BUFFERED | _FILE_WRITE);
    _std_stream_init(STDERR_FILENO, &_stderr, &stderr, _stderr_buff, _FILE_UNBUFFERED | _FILE_WRITE);
    errno = EOK;
}
