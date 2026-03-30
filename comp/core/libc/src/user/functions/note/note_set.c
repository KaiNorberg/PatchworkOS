#include <user/common/note.h>

#include <libc/proc.h>

#include <errno.h>

status_t note_at(note_func_t func, note_act_t action)
{
    if (func == NULL)
    {
        return ERR(LIBSTD, INVAL);
    }

    switch (action)
    {
    case NOTE_ADD:
    {
        if (!_note_handler_add(func))
        {
            return ERR(LIBSTD, NOSPACE);
        }
    }
    break;
    case NOTE_REMOVE:
        _note_handler_remove(func);
        break;
    default:
        return ERR(LIBSTD, INVAL);
    }

    return OK;
}