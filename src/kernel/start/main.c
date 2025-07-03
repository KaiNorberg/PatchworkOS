#include "kernel.h"
#include "log/log.h"
#include "mem/heap.h"
#include "mem/slab.h"
#include "sched/loader.h"
#include "sched/sched.h"
#include "utils/ring.h"

#include <assert.h>
#include <boot/boot_info.h>
#include <stdio.h>
#include <string.h>

void print_tree(file_t* file, const char* path, uint64_t depth)
{
    uint64_t totalEntries = vfs_getdirent(file, NULL, 0);
    if (totalEntries == ERR)
    {
        LOG_ERR("print_tree: error reading directory (%s)\n", strerror(errno));
        return;
    }

    if (totalEntries == 0)
    {
        return;
    }

    dirent_t* entries = (dirent_t*)heap_alloc(totalEntries * sizeof(dirent_t), HEAP_NONE);
    if (entries == NULL)
    {
        LOG_ERR("print_tree: failed to allocate memory for directory entries (%s)\n", strerror(errno));
        return;
    }

    uint64_t entriesRead = vfs_getdirent(file, entries, totalEntries);
    if (entriesRead == ERR)
    {
        LOG_ERR("print_tree: error reading directory entries (%s)\n", strerror(errno));
        heap_free(entries);
        return;
    }

    for (uint64_t i = 0; i < totalEntries; ++i)
    {
        for (uint64_t j = 0; j < depth; ++j)
        {
            LOG_INFO("│   ");
        }
        LOG_INFO("├── %s\n", entries[i].name);

        if (entries[i].type == INODE_DIR)
        {
            char next[MAX_PATH];
            snprintf(next, MAX_PATH, "%s/%s", path, entries[i].name);
            char nextWithFlags[MAX_PATH];
            snprintf(nextWithFlags, MAX_PATH, "%s/%s?dir", path, entries[i].name);

            file_t* nextFile = vfs_open(nextWithFlags);
            if (nextFile == NULL)
            {
                LOG_ERR("print_tree: error opening file (%s)\n", next);
            }
            else
            {
                print_tree(nextFile, next, depth + 1);
                file_deref(nextFile);
            }
        }
    }

    heap_free(entries);
}

void main(boot_info_t* bootInfo)
{
    kernel_init(bootInfo);

    stat_t stat;
    if (vfs_stat("/startup.nsh", &stat) == ERR)
    {
        LOG_ERR("main: stat err %s\n", strerror(errno));
    }
    else
    {
        LOG_INFO("##Stat Output##\nNumber: %u\nType: %u\nSize: %lu\nBlocks: %lu\nLink Amount: %lu\nAccess "
                 "Time: %u\nModify "
                 "Time: %u\nChange Time: %u\nName: %s\n\n",
            stat.number, stat.type, stat.size, stat.blocks, stat.linkAmount, stat.accessTime,
            stat.modifyTime, stat.changeTime, stat.name);
    }

    file_t* rootDir = vfs_open("/?dir");
    if (rootDir == NULL)
    {
        LOG_ERR("main: failed to open root (%s)\n", strerror(errno));
    }
    else
    {
        LOG_INFO("##Vfs Tree##\n");
        print_tree(rootDir, "/", 0);
        file_deref(rootDir);
        LOG_INFO("\n");
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
