#include "tss.h"

#include "string/string.h"
#include "process/process.h"
#include "global_heap/global_heap.h"
#include "tty/tty.h"

TaskStateSegment* tss;

void tss_init()
{    
    tty_start_message("TSS loading");

    tss = gmalloc(1, PAGE_DIR_READ_WRITE);
    memset(tss, 0, 0x1000);
    tss->rsp0 = (uint64_t)gmalloc(1, PAGE_DIR_READ_WRITE) + 0x1000;
    tss->rsp1 = tss->rsp0;
    tss->rsp2 = tss->rsp0;
    tss->iopb = sizeof(TaskStateSegment);
    
    tty_end_message(TTY_MESSAGE_OK);
}