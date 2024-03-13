#include "tss.h"

#include "heap/heap.h"

Tss* tss_new(void)
{
    Tss* tss = kmalloc(sizeof(Tss));

    tss->rsp0 = 0;
    tss->rsp1 = 0;
    tss->rsp2 = 0;
    tss->iopb = sizeof(Tss);

    return tss;
}