#include "stdlib_internal/init.h"

#include "heap.h"
#include "thrd.h"

#ifdef __EMBED__
void _StdInit(void)
{
    _HeapInit();
}
#else
void _StdInit(void)
{
    _HeapInit();
    _ThrdInit();
}
#endif
