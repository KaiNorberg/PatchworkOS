#include "fs/path.h"
#include "fs/vfs.h"
#include "kernel.h"
#include "log/log.h"
#include "log/panic.h"
#include "mem/heap.h"
#include "sched/loader.h"
#include "sched/sched.h"
#include "utils/ref.h"

#include <_internal/MAX_PATH.h>
#include <assert.h>
#include <boot/boot_info.h>
#include <stdio.h>
#include <string.h>

void kmain(boot_info_t* bootInfo)
{
    kernel_init(bootInfo);

    LOG_INFO("spawning init thread\n");
    const char* argv[] = {"/bin/init", NULL};
    thread_t* initThread = loader_spawn(argv, PRIORITY_MAX_USER - 2, NULL);
    if (initThread == NULL)
    {
        panic(NULL, "Failed to spawn init thread");
    }

    // Set klog as stdout for init process
    file_t* klog = vfs_open(PATHNAME("/dev/klog"));
    if (klog == NULL)
    {
        panic(NULL, "Failed to open klog");
    }
    if (vfs_ctx_openas(&initThread->process->vfsCtx, STDOUT_FILENO, klog) == ERR)
    {
        panic(NULL, "Failed to set klog as stdout for init process");
    }
    ref_dec(klog);

    sched_push(initThread, NULL);

    LOG_INFO("done with boot thread\n");
    sched_done_with_boot_thread();
}
