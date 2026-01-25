#include <user/common/note.h>

#include <sys/proc.h>

#include <errno.h>

uint64_t atnotify(atnotify_func_t func, atnotify_t action)
{
    if (func == NULL)
    {
        errno = EINVAL;
        return _FAIL;
    }

    switch (action)
    {
    case ATNOTIFY_ADD:
        if (_note_handler_add(func) == _FAIL)
        {
            errno = ENOMEM;
            return _FAIL;
        }
        break;
    case ATNOTIFY_REMOVE:
        _note_handler_remove(func);
        break;
    default:
        errno = EINVAL;
        return _FAIL;
    }

    return 0;
}