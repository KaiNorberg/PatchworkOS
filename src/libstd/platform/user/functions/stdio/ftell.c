#include <errno.h>
#include <limits.h>
#include <stdio.h>

#include "platform/user/common/file.h"

long int ftell(FILE* stream)
{
    /* ftell() must take into account:
       - the actual *physical* offset of the file, i.e. the offset as recognized
         by the operating system (and stored in stream->pos.offset); and
       - any buffers held by PDCLib, which
         - in case of unwritten buffers, count in *addition* to the offset; or
         - in case of unprocessed pre-read buffers, count in *substraction* to
           the offset. (Remember to count ungetidx into this number.)
       Conveniently, the calculation ( ( bufend - bufidx ) + ungetidx ) results
       in just the right number in both cases:
         - in case of unwritten buffers, ( ( 0 - unwritten ) + 0 )
           i.e. unwritten bytes as negative number
         - in case of unprocessed pre-read, ( ( preread - processed ) + unget )
           i.e. unprocessed bytes as positive number.
       That is how the somewhat obscure return-value calculation works.
    */
    /*  If offset is too large for return type, report error instead of wrong
        offset value.
    */
    long int result;
    mtx_lock(&stream->mtx);

    if ((stream->pos.offset - stream->bufEnd) > (LONG_MAX - (stream->bufIndex - stream->ungetIndex)))
    {
        /* integer overflow */
        mtx_unlock(&stream->mtx);
        errno = ERANGE;
        return -1;
    }

    result = (stream->pos.offset - (((int)stream->bufEnd - (int)stream->bufIndex) + stream->ungetIndex));
    mtx_unlock(&stream->mtx);
    return result;
}
