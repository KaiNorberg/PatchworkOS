#include "scon_priv.h"

bool scon_is_atom(scon_item_t* ref)
{
    return ref != NULL && ref->type == SCON_ATOM;
}