#include "tss.h"

#include "interrupt_stack/interrupt_stack.h"
#include "string/string.h"
#include "process/process.h"

__attribute__((aligned(0x1000)))
TaskStateSegment tss;

void tss_init()
{
    memset(&tss, 0, sizeof(TaskStateSegment));
    tss.rsp0 = (uint64_t)interrupt_stack_get_top();
    tss.rsp1 = tss.rsp0;
    tss.rsp2 = tss.rsp0;
    tss.iopb = sizeof(TaskStateSegment);

    tss_load();
}