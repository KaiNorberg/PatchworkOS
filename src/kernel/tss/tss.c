#include "tss.h"

#include "string/string.h"
#include "process/process.h"
#include "global_heap/global_heap.h"

__attribute__((aligned(0x1000)))
TaskStateSegment tss;

void tss_init()
{
    memset(&tss, 0, sizeof(TaskStateSegment));
    tss.rsp0 = (uint64_t)gmalloc(1, PAGE_DIR_READ_WRITE);
    tss.rsp1 = tss.rsp0;
    tss.rsp2 = tss.rsp0;
    tss.iopb = sizeof(TaskStateSegment);

    tss_load();
}