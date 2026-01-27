#include <user/common/note.h>

#include <sys/proc.h>

#include <errno.h>

status_t atnotify(atnotify_func_t func, atnotify_t action)
{
    if (func == NULL)
    {
        return ERR(LIBSTD, INVAL);
    }

    switch (action)
    {
    case ATNOTIFY_ADD:
    {
        if (!_note_handler_add(func))
        {
            return ERR(LIBSTD, NOSPACE);
        }
    }
    break;
    case ATNOTIFY_REMOVE:
        _note_handler_remove(func);
        break;
    default:
        return ERR(LIBSTD, INVAL);
    }

    return OK;
}