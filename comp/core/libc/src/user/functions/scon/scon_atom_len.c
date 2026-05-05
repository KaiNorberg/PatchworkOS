#include "scon_priv.h"

size_t scon_atom_len(scon_item_t* ref)
{
    if (ref == NULL || ref->type != SCON_ATOM)
    {
        return 0;
    }
    return ref->flags & SCON_ITEM_DYNAMIC ? ref->dstr.length : ref->atom.length;
}