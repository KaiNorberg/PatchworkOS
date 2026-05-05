#include "scon_priv.h"

bool scon_atom_eq(scon_item_t* ref, const char* str)
{
    if (ref == NULL || ref->type != SCON_ATOM)
    {
        return false;
    }

    size_t len = scon_atom_len(ref);
    if (len != strlen(str))
    {
        return false;
    }
    return strncmp(scon_atom_str(ref), str, len) == 0;
}