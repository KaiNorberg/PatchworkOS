#include "scon_priv.h"

const char* scon_atom_str(scon_item_t* ref)
{
    if (ref == NULL || ref->type != SCON_ATOM)
    {
        return NULL;
    }
    return ref->flags & SCON_ITEM_DYNAMIC ? ref->dstr.data : ref->atom.str;
}