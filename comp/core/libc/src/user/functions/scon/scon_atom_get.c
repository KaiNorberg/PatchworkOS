#include "scon_priv.h"

status_t scon_atom_get(scon_item_t* ref, const char** out, size_t* len)
{
    if (ref == NULL || ref->type != SCON_ATOM)
    {
        return ERR(LIBSTD, INVAL);
    }

    if (ref->flags & SCON_ITEM_DYNAMIC)
    {
        *out = ref->dstr.data;
        *len = ref->dstr.length;
    }
    else
    {
        *out = ref->atom.str;
        *len = ref->atom.length;
    }
    return OK;
}