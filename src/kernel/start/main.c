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

static void print_directory(const char* path)
{

}

void main(boot_info_t* bootInfo)
{
    kernel_init(bootInfo);

    stat_t stat;
    if (vfs_stat("/startup.nsh", &stat) == ERR)
    {
        LOG_INFO("main: stat err %s\n", strerror(errno));
    }
    else
    {
        LOG_INFO("Number: %u\nType: %u\nFlags: %u\nSize: %lu\nBlocks: %lu\nLink Amount: %lu\nAccess Time: %u\nModify Time: %u\nChange Time: %u\nName: %s\n",
            stat.number, stat.type, stat.flags, stat.size, stat.blocks, stat.linkAmount,
            stat.accessTime, stat.modifyTime, stat.changeTime, stat.name);
    }

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
