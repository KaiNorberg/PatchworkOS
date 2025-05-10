#include <string.h>

#include <locale.h>

int strcoll(const char* s1, const char* s2)
{
    /* FIXME: This should access _PDCLIB_lc_collate. */
    return strcmp(s1, s2);
}
