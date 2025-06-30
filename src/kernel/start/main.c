#include "kernel.h"
#include "log/log.h"
#include "mem/heap.h"
#include "mem/slab.h"
#include "sched/loader.h"
#include "sched/sched.h"
#include "utils/ring.h"

#include <assert.h>
#include <boot/boot_info.h>
#include <string.h>

void main(boot_info_t* bootInfo)
{
    kernel_init(bootInfo);

    LOG_INFO("looping\n");
    while (1)
        ;

    const char* argv[] = {"home:/bin/init", NULL};
    thread_t* initThread = loader_spawn(argv, PRIORITY_MAX_USER - 2, NULL);
    assert(initThread != NULL);

    // Set klog as stdout for init process
    file_t* klog = vfs_open("sys:/klog");
    assert(klog != NULL);
    assert(vfs_ctx_openas(&initThread->process->vfsCtx, STDOUT_FILENO, klog) != ERR);
    file_deref(klog);

    sched_push(initThread, NULL, NULL);

    sched_done_with_boot_thread();
}
