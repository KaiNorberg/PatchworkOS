#include "init.h"

#include "heap.h"

#ifdef __KERNEL__
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