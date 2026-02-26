#include <kernel/fs/diremit.h>

#include <kernel/mem/mdl.h>
#include <string.h>

bool diremit(diremit_t* emit, const char* name)
{
    size_t len = strlen(name) + 1;

    if (emit->currentPos + len <= emit->offset)
    {
        emit->currentPos += len;
        return true;
    }

    size_t skip = 0;
    if (emit->currentPos < emit->offset)
    {
        skip = emit->offset - emit->currentPos;
    }

    size_t toWrite = len - skip;
    size_t copied = 0;

    status_t status =
        mdl_copy_in(irp_current(emit->irp)->read.buffer, toWrite, emit->bytes, &copied, name + skip, toWrite);
    if (IS_ERR(status))
    {
        emit->status = status;
        return false;
    }

    emit->bytes += copied;
    emit->currentPos += len;

    if (copied != toWrite)
    {
        emit->status = OK;
        return false;
    }

    return true;
}
