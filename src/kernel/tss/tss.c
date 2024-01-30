#include "tss.h"

#include "string/string.h"
#include "process/process.h"
#include "global_heap/global_heap.h"
#include "tty/tty.h"
#include "page_allocator/page_allocator.h"
#include "debug/debug.h"

Tss* tss_new()
{
    Tss* tss = gmalloc(1);

    tss->rsp0 = (uint64_t)gmalloc(1) + 0x1000;
    tss->rsp1 = tss->rsp0;
    tss->rsp2 = tss->rsp0;
    tss->iopb = sizeof(Tss);

    return tss;
}