#include <user/common/note.h>

#include <sys/proc.h>

#include <errno.h>

uint64_t atnotify(atnotify_func_t func, atnotify_t action)
{
    if (func == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    switch (action)
    {
    case ATNOTIFY_ADD:
        if (_note_handler_add(func) == ERR)
        {
            errno = ENOMEM;
            return ERR;
        }
        break;
    case ATNOTIFY_REMOVE:
        _note_handler_remove(func);
        break;
    default:
        errno = EINVAL;
        return ERR;
    }

    return 0;
}