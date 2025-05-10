#include "common/use_annex_k.h"
#include <string.h>

size_t strerrorlen_s(errno_t errnum)
{
    return strlen(strerror(errnum));
}
