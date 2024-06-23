#include "stdlib_internal/init.h"

#include "heap.h"

#ifdef __EMBED__
void _StdInit(void)
{
    _HeapInit();
}
#else
void _StdInit(void)
{
    _HeapInit();
}
#endif
