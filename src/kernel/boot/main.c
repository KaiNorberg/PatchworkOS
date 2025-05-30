#include "kernel.h"
#include "sched/loader.h"
#include "sched/sched.h"
#include "utils/log.h"
#include "utils/ring.h"

#include <assert.h>
#include <bootloader/boot_info.h>
#include <stdio.h>
#include <string.h>

void main(boot_info_t* bootInfo)
{
    kernel_init(bootInfo);

    const char* argv[] = {"home:/bin/init", NULL};
    thread_t* initThread = loader_spawn(argv, PRIORITY_MIN + 1, NULL);
    assert(initThread != NULL);

    // Set klog as stdout for init process
    file_t* klog = vfs_open(PATH(sched_process(), "sys:/klog"));
    assert(klog != NULL);
    assert(vfs_ctx_openas(&initThread->process->vfsCtx, STDOUT_FILENO, klog) != ERR);
    file_deref(klog);

    sched_push(initThread);

    // Exit boot thread
    sched_thread_exit();
    while (1)
        ;
}
