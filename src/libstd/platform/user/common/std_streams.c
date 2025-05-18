#include "std_streams.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/io.h>

static uint8_t _StdinBuff[BUFSIZ];
static uint8_t _StdoutBuff[BUFSIZ];
static uint8_t _StderrBuff[BUFSIZ];

static FILE _Stdin;
static FILE _Stdout;
static FILE _Stderr;

FILE* stdin;
FILE* stdout;
FILE* stderr;

static void _StdStreamInit(fd_t fd, FILE* stream, FILE** streamPtr, void* buffer, _FileFlags_t flags)
{
    memset(stream, 0, sizeof(FILE));
    list_entry_init(&stream->entry);

    if (_FileInit(stream, fd, flags, buffer, BUFSIZ) == ERR)
    {
        exit(EXIT_FAILURE);
    }

    _FilesPush(stream);
    *streamPtr = stream;
}

void _StdStreamsInit(void)
{
    _StdStreamInit(STDIN_FILENO, &_Stdin, &stdin, _StdinBuff, _FILE_LINE_BUFFERED | _FILE_READ);
    _StdStreamInit(STDOUT_FILENO, &_Stdout, &stdout, _StdoutBuff, _FILE_LINE_BUFFERED | _FILE_WRITE);
    _StdStreamInit(STDERR_FILENO, &_Stderr, &stderr, _StderrBuff, _FILE_UNBUFFERED | _FILE_WRITE);
    errno = 0;
}
