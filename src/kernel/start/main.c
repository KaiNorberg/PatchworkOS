#include "fs/path.h"
#include "fs/vfs.h"
#include "kernel.h"
#include "log/log.h"
#include "mem/heap.h"
#include "sched/loader.h"
#include "sched/sched.h"

#include <_internal/MAX_PATH.h>
#include <assert.h>
#include <boot/boot_info.h>
#include <stdio.h>
#include <string.h>

void main(boot_info_t* bootInfo)
{
    kernel_init(bootInfo);

    LOG_INFO("main: spawning init thread\n");
    const char* argv[] = {"/bin/init", NULL};
    thread_t* initThread = loader_spawn(argv, PRIORITY_MAX_USER - 2, NULL);
    assert(initThread != NULL);

    // Set klog as stdout for init process
    file_t* klog = vfs_open(PATHNAME("/dev/klog"));
    assert(klog != NULL);
    assert(vfs_ctx_openas(&initThread->process->vfsCtx, STDOUT_FILENO, klog) != ERR);
    file_deref(klog);

    sched_push(initThread, NULL, NULL);

    LOG_INFO("main: done\n");
    sched_done_with_boot_thread();
}
