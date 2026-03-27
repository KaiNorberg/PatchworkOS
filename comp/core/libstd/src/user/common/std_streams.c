#include "std_streams.h"
#include "file.h"

#include <errno.h>
#include <libstd/fs.h>
#include <libstd/io.h>
#include <libstd/proc.h>
#include <stdlib.h>
#include <string.h>

static uint8_t _stdinBuff[BUFSIZ];
static uint8_t _stdoutBuff[BUFSIZ];
static uint8_t _stderrBuff[BUFSIZ];

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

    if (_file_init(stream, fd, flags, buffer, BUFSIZ) == EOF)
    {
        proc_exit(IOFMT("libstd: failed to initialize standard stream (fd=%d)\n", fd));
    }

    *streamPtr = stream;
}

void _std_streams_init(void)
{
    _std_stream_init(FDIN, &_stdin, &stdin, _stdinBuff, _FILE_LINE_BUFFERED | _FILE_READ);
    _std_stream_init(FDOUT, &_stdout, &stdout, _stdoutBuff, _FILE_LINE_BUFFERED | _FILE_WRITE);
    _std_stream_init(FDERR, &_stderr, &stderr, _stderrBuff, _FILE_UNBUFFERED | _FILE_WRITE);
    errno = EOK;
}
