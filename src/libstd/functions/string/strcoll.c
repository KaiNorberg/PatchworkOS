#include <string.h>

int strcoll(const char* s1, const char* s2)
{
    /* FIXME: This should access _pdclib_lc_collate. */
    return strcmp(s1, s2);
}
