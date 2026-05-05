#include <kernel/fs/binding.h>
#include <kernel/fs/ctl.h>
#include <kernel/fs/dentry.h>
#include <kernel/fs/file.h>
#include <kernel/fs/filesystem.h>
#include <kernel/fs/path.h>
#include <kernel/fs/procfs.h>
#include <kernel/fs/vfs.h>
#include <kernel/fs/vnode.h>
#include <kernel/io/irp.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/mem/vmm.h>
#include <kernel/proc/process.h>
#include <kernel/sched/sched.h>
#include <kernel/sched/thread.h>
#include <kernel/sync/lock.h>
#include <kernel/sync/rcu.h>

#include <assert.h>
#include <libc/fs.h>
#include <libc/io.h>
#include <libc/list.h>
#include <libc/math.h>
#include <libc/status.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static dentry_t* root = NULL;

static process_t* procfs_get_process(irp_t* irp)
{
    vnode_t* vnode = irp_current(irp)->vnode;
    if (vnode->data == NULL)
    {
        return irp->process;
    }
    return vnode->data;
}

static status_t procfs_prio_read(irp_t* irp)
{
    process_t* process = procfs_get_process(irp);

    prio_t priority = atomic_load(&process->priority);

    char prioStr[MAX_NAME];
    uint32_t length = snprintf(prioStr, MAX_NAME, "%llu", priority);
    return irp_read_helper(irp, prioStr, length);
}

static status_t procfs_prio_write(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    process_t* process = procfs_get_process(irp);

    char prioStr[MAX_NAME];
    size_t bytesWritten;
    status_t status = sglist_copy_out(frame->write.buffer, SIZE_MAX, 0, &bytesWritten, prioStr, MAX_NAME - 1);
    if (IS_ERR(status))
    {
        return status;
    }
    prioStr[bytesWritten] = '\0';

    long long int prio = atoll(prioStr);
    if (prio < 0)
    {
        return ERR(FS, INVAL);
    }
    if (prio > PRIO_MAX_USER)
    {
        return ERR(FS, ACCESS);
    }

    atomic_store(&process->priority, prio);
    irp->result = bytesWritten;
    return OK;
}

static vnode_class_t prioClass = {
    .name = "procfs prio",
    .type = FILE_TYPE_SYSTEM,
    .handlers =
        {
            VNODE_HANDLERS(),
            [IRP_MJ_READ] = procfs_prio_read,
            [IRP_MJ_WRITE] = procfs_prio_write,
        },
};

static status_t procfs_note_write(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    process_t* process = procfs_get_process(irp);

    size_t count = sglist_size(frame->write.buffer);
    if (count == 0)
    {
        irp->result = 0;
        return OK;
    }

    if (count >= NOTE_MAX)
    {
        return ERR(FS, INVAL);
    }

    char string[NOTE_MAX];
    size_t bytesWritten;
    status_t status = sglist_copy_out(frame->write.buffer, SIZE_MAX, 0, &bytesWritten, string, NOTE_MAX - 1);
    if (IS_ERR(status))
    {
        return status;
    }
    string[bytesWritten] = '\0';

    job_t* job = job_get(&process->job);
    if (job == NULL)
    {
        return ERR(FS, DYING);
    }
    UNREF_DEFER(job);

    if (job_is_leader(job, &process->job))
    {
        job_send_note(job, string);
    }
    else
    {
        process_send_note(process, string);
    }

    irp->result = bytesWritten;
    return OK;
}

static vnode_class_t noteClass = {
    .name = "procfs note",
    .type = FILE_TYPE_SYSTEM,
    .handlers =
        {
            VNODE_HANDLERS(),
            [IRP_MJ_WRITE] = procfs_note_write,
        },
};

static status_t procfs_pid_read(irp_t* irp)
{
    process_t* process = procfs_get_process(irp);

    char pidStr[MAX_NAME];
    uint32_t length = snprintf(pidStr, MAX_NAME, "%llu", process->id);
    return irp_read_helper(irp, pidStr, length);
}

static vnode_class_t pidClass = {.name = "procfs pid",
    .type = FILE_TYPE_SYSTEM,
    .handlers = {
        VNODE_HANDLERS(),
        [IRP_MJ_READ] = procfs_pid_read,
    }};

static status_t procfs_wait_cancel(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    process_t* process = procfs_get_process(irp);

    lock_acquire(&process->dyingIrpsLock);
    if (list_contains(&irp->entry))
    {
        list_remove(&irp->entry);
    }
    lock_release(&process->dyingIrpsLock);

    return OK;
}

static status_t procfs_wait_read(irp_t* irp)
{
    process_t* process = procfs_get_process(irp);

    if (!(atomic_load(&process->flags) & PROCESS_DYING))
    {
        LOCK_SCOPE(&process->dyingIrpsLock);
        return irp_delay(irp, &process->dyingIrps, procfs_wait_cancel);
    }

    lock_acquire(&process->result.lock);
    status_t status = irp_read_helper(irp, process->result.buffer, strlen(process->result.buffer));
    lock_release(&process->result.lock);
    return status;
}

static status_t procfs_wait_poll(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    process_t* process = procfs_get_process(irp);

    if (!(atomic_load(&process->flags) & PROCESS_DYING))
    {
        LOCK_SCOPE(&process->dyingIrpsLock);
        return irp_delay(irp, &process->dyingIrps, procfs_wait_cancel);
    }

    irp->result = IOEVENT_READ;
    return OK;
}

static vnode_class_t waitClass = {
    .name = "procfs wait",
    .type = FILE_TYPE_SYSTEM,
    .handlers =
        {
            VNODE_HANDLERS(),
            [IRP_MJ_READ] = procfs_wait_read,
            [IRP_MJ_POLL] = procfs_wait_poll,
        },
};

static status_t procfs_perf_read(irp_t* irp)
{
    process_t* process = procfs_get_process(irp);
    size_t userPages = space_user_page_count(&process->space);

    lock_acquire(&process->threads.lock);
    size_t threadCount = process->threads.count;
    lock_release(&process->threads.lock);

    clock_t userClocks = atomic_load(&process->perf.userClocks);
    clock_t kernelClocks = atomic_load(&process->perf.kernelClocks);
    clock_t startTime = process->perf.startTime;

    char statStr[MAX_NAME];
    int length = snprintf(statStr, sizeof(statStr),
        "user_clocks %llu\nkernel_sched_clocks %llu\nstart_clocks %llu\nuser_pages %llu\nthread_count %llu", userClocks,
        kernelClocks, startTime, userPages, threadCount);
    if (length < 0)
    {
        return ERR(FS, IMPL);
    }

    return irp_read_helper(irp, statStr, length);
}

static vnode_class_t perfClass = {
    .name = "procfs perf",
    .type = FILE_TYPE_SYSTEM,
    .handlers =
        {
            VNODE_HANDLERS(),
            [IRP_MJ_READ] = procfs_perf_read,
        },
};

static status_t procfs_ctl_control(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    process_t* process = procfs_get_process(irp);
    iocmd_t cmd = frame->control.command;
    const char* args = frame->control.args;

    switch (cmd)
    {
    case IOCMD('c', 'l', 'o', 's', 'e'):
    {
        fd_t fd1;
        fd_t fd2;
        int count = sscanf(args, "%lld %lld", &fd1, &fd2);
        if (count == 1)
        {
            return file_table_drop(&process->files, fd1);
        }
        if (count == 2)
        {
            file_table_drop_range(&process->files, fd1, fd2);
            return OK;
        }
        return ERR(FS, INVAL);
    }
    case IOCMD('d', 'u', 'p'):
    {
        fd_t oldFd;
        fd_t newFd;
        if (sscanf(args, "%lld %lld", &oldFd, &newFd) != 2)
        {
            return ERR(FS, INVAL);
        }
        return file_table_dup(&process->files, oldFd, &newFd);
    }
    case IOCMD('g', 'i', 'v', 'e'):
    {
        fd_t parentFd;
        fd_t childFd;
        if (sscanf(args, "%lld %lld", &parentFd, &childFd) != 2)
        {
            return ERR(FS, INVAL);
        }

        file_t* file = file_table_get(&irp->process->files, parentFd);
        if (file == NULL)
        {
            return ERR(FS, BADFD);
        }

        status_t status = OK;
        if (!file_table_set(&process->files, childFd, file))
        {
            status = ERR(FS, INVAL);
        }

        UNREF(file);
        return status;
    }
    case IOCMD('s', 't', 'a', 'r', 't'):
    {
        lock_acquire(&process->threads.lock);
        if (process->threads.count > 0)
        {
            lock_release(&process->threads.lock);
            return ERR(FS, RUNNING);
        }
        lock_release(&process->threads.lock);

        if (atomic_load(&process->flags) & PROCESS_DYING)
        {
            return ERR(FS, DYING);
        }

        uintptr_t entry;
        uintptr_t stack;
        if (sscanf(args, "%zu %zu", &entry, &stack) != 2)
        {
            return ERR(FS, INVAL);
        }

        thread_t* thread;
        status_t status = thread_new(&thread, process);
        if (IS_ERR(status))
        {
            return status;
        }

        thread->frame.rip = entry;
        thread->frame.rsp = stack;
        thread->frame.rbp = stack;
        thread->frame.cs = GDT_CS_RING3;
        thread->frame.ss = GDT_SS_RING3;
        thread->frame.rflags = RFLAGS_ALWAYS_SET | RFLAGS_INTERRUPT_ENABLE;

        sched_submit(thread);
        return OK;
    }
    case IOCMD('k', 'i', 'l', 'l'):
    {
        if (args[0] == '\0')
        {
            process_kill(process, "killed");
            return OK;
        }
        process_kill(process, args);
        return OK;
    }
    default:
        return ERR(FS, INVALCTL);
    }
}

static vnode_class_t ctlClass = {
    .name = "procfs ctl",
    .type = FILE_TYPE_SYSTEM,
    .handlers =
        {
            VNODE_HANDLERS(),
            [IRP_MJ_WRITE] = ctl_generic_write,
            [IRP_MJ_CONTROL] = procfs_ctl_control,
        },
};

static status_t procfs_mem_read(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    process_t* process = procfs_get_process(irp);

    size_t addr = *frame->read.offset;
    size_t count = sglist_size(frame->read.buffer);
    size_t copied = 0;

    if (addr >= process->space.endAddress || addr < process->space.startAddress)
    {
        irp->result = 0;
        return INFO(FS, EOF);
    }

    if (addr + count > process->space.endAddress || addr + count < addr)
    {
        count = process->space.endAddress - addr;
    }

    status_t status = sglist_copy_in_space(frame->read.buffer, count, 0, &copied, &process->space, (const void*)addr);

    *frame->read.offset = addr + copied;
    irp->result = copied;

    if (copied > 0)
    {
        if (*frame->read.offset >= process->space.endAddress)
        {
            return INFO(FS, EOF);
        }
        return OK;
    }

    return status;
}

static status_t procfs_mem_write(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    process_t* process = procfs_get_process(irp);

    size_t addr = *frame->write.offset;
    size_t count = sglist_size(frame->write.buffer);
    size_t copied = 0;

    if (addr >= process->space.endAddress || addr < process->space.startAddress)
    {
        irp->result = 0;
        return OK;
    }

    if (addr + count > process->space.endAddress || addr + count < addr)
    {
        count = process->space.endAddress - addr;
    }

    status_t status = sglist_copy_out_space(frame->write.buffer, count, 0, &copied, &process->space, (void*)addr);

    *frame->write.offset = addr + copied;
    irp->result = copied;

    if (copied > 0)
    {
        return OK;
    }

    return status;
}

static status_t procfs_mem_seek(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    process_t* process = procfs_get_process(irp);

    return irp_seek_helper(irp, process->space.endAddress);
}

static status_t procfs_mem_mmap(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    process_t* process = procfs_get_process(irp);

    size_t offset = frame->mmap.offset;
    size_t length = frame->mmap.length;

    if (offset % PAGE_SIZE != 0)
    {
        return ERR(FS, INVAL);
    }

    size_t pageAmount = BYTES_TO_PAGES(length);
    if (pageAmount == 0)
    {
        return ERR(FS, INVAL);
    }

    if (!space_check_access(&process->space, (void*)offset, length))
    {
        return ERR(FS, FAULT);
    }

    pfn_t* pfns = malloc(pageAmount * sizeof(pfn_t));
    if (pfns == NULL)
    {
        return ERR(FS, NOMEM);
    }

    for (size_t i = 0; i < pageAmount; i++)
    {
        phys_addr_t phys;
        status_t status = space_virt_to_phys_alloc(&process->space, (void*)(offset + (i * PAGE_SIZE)), &phys);
        if (IS_ERR(status))
        {
            free(pfns);
            return status;
        }
        pfns[i] = PHYS_TO_PFN(phys);
    }

    void* addr = frame->mmap.address;
    status_t status = vmm_map_shared(&irp->process->space, &addr, pfns, pageAmount, frame->mmap.flags);
    free(pfns);
    if (IS_ERR(status))
    {
        return status;
    }

    irp->result = (uintptr_t)addr;
    return OK;
}

static vnode_class_t memClass = {
    .name = "procfs mem",
    .type = FILE_TYPE_SYSTEM,
    .handlers =
        {
            VNODE_HANDLERS(),
            [IRP_MJ_READ] = procfs_mem_read,
            [IRP_MJ_WRITE] = procfs_mem_write,
            [IRP_MJ_SEEK] = procfs_mem_seek,
            [IRP_MJ_MMAP] = procfs_mem_mmap,
        },
};

typedef struct
{
    const char* name;
    vnode_class_t* cls;
} procfs_entry_t;

static const procfs_entry_t dirEntries[] = {
    {"prio", &prioClass},
    {"note", &noteClass},
    {"pid", &pidClass},
    {"wait", &waitClass},
    {"perf", &perfClass},
    {"ctl", &ctlClass},
    {"mem", &memClass},
};

static status_t procfs_dir_lookup(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    process_t* process = frame->vnode->data;

    dentry_t* target = frame->lookup.dentry;

    for (size_t i = 0; i < ARRAY_SIZE(dirEntries); i++)
    {
        if (!dstr_eq(&target->name, dirEntries[i].name, strlen(dirEntries[i].name)))
        {
            continue;
        }

        file_number_t number = vnode_hash(frame->vnode->number, dirEntries[i].name);
        vnode_t* vnode = vnode_new(frame->vnode->volume, dirEntries[i].cls, number);
        if (vnode == NULL)
        {
            return ERR(FS, NOMEM);
        }
        UNREF_DEFER(vnode);
        vnode->data = process;

        dentry_make_positive(target, vnode);
        return OK;
    }

    return INFO(FS, NEGATIVE);
}

static status_t procfs_dir_read(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);

    diremit_t emit;
    diremit_begin(&emit, irp);

    if (!diremit(&emit, "."))
    {
        goto end;
    }

    if (!diremit(&emit, ".."))
    {
        goto end;
    }

    for (size_t i = 0; i < ARRAY_SIZE(dirEntries); i++)
    {
        if (!diremit(&emit, dirEntries[i].name))
        {
            goto end;
        }
    }

end:
    return diremit_end(&emit);
}

static status_t procfs_dir_reclaim(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    process_t* process = frame->vnode->data;

    if (process != NULL)
    {
        UNREF(process);
    }
    frame->vnode->data = NULL;
    return OK;
}

static vnode_class_t dirClass = {
    .name = "procfs dir",
    .type = FILE_TYPE_DIRECTORY,
    .handlers =
        {
            VNODE_DIR_HANDLERS(),
            [IRP_MJ_LOOKUP] = procfs_dir_lookup,
            [IRP_MJ_READ] = procfs_dir_read,
            [IRP_MJ_RECLAIM] = procfs_dir_reclaim,
        },
};

static status_t procfs_file_clone_open(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    file_t* file = frame->file;
    if (file == NULL)
    {
        return ERR(FS, EXPECT_FILE);
    }

    process_t* parent = irp->process;
    job_t* job = job_get(&parent->job);
    if (job == NULL)
    {
        return ERR(FS, DYING);
    }
    UNREF_DEFER(job);

    process_t* child;
    status_t status = process_new(&child, PRIO_DEFAULT, job);
    if (IS_ERR(status))
    {
        return status;
    }
    UNREF_DEFER(child);

    vnode_t* vnode = vnode_new(frame->vnode->volume, &dirClass, vnode_hash(child->id, "process"));
    if (vnode == NULL)
    {
        return ERR(FS, NOMEM);
    }
    UNREF_DEFER(vnode);
    vnode->data = REF(child);

    dentry_t* dentry = dentry_new(NULL, "process", 8);
    if (dentry == NULL)
    {
        return ERR(FS, NOMEM);
    }
    UNREF_DEFER(dentry);

    dentry_make_positive(dentry, vnode);
    return file_redirect(file, dentry);
}

static vnode_class_t fileCloneClass = {
    .name = "procfs clone file",
    .type = FILE_TYPE_SYSTEM,
    .handlers =
        {
            VNODE_HANDLERS(),
            [IRP_MJ_OPEN] = procfs_file_clone_open,
        },
};

static const procfs_entry_t rootEntries[] = {
    {"clone", &fileCloneClass},
};

static status_t procfs_root_lookup(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    dentry_t* target = frame->lookup.dentry;

    for (size_t i = 0; i < ARRAY_SIZE(rootEntries); i++)
    {
        if (!dstr_eq(&target->name, rootEntries[i].name, strlen(rootEntries[i].name)))
        {
            continue;
        }

        file_number_t number = vnode_hash(frame->vnode->number, rootEntries[i].name);
        vnode_t* vnode = vnode_new(frame->vnode->volume, rootEntries[i].cls, number);
        if (vnode == NULL)
        {
            return ERR(FS, NOMEM);
        }
        UNREF_DEFER(vnode);
        vnode->data = NULL;

        dentry_make_positive(target, vnode);
        return OK;
    }

    return INFO(FS, NEGATIVE);
}

static status_t procfs_root_read(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);

    diremit_t emit;
    diremit_begin(&emit, irp);

    if (!diremit(&emit, "."))
    {
        goto end;
    }

    if (!diremit(&emit, ".."))
    {
        goto end;
    }

    for (size_t i = 0; i < ARRAY_SIZE(rootEntries); i++)
    {
        if (!diremit(&emit, rootEntries[i].name))
        {
            break;
        }
    }

end:
    return diremit_end(&emit);
}

static vnode_class_t rootClass = {
    .name = "procfs root",
    .type = FILE_TYPE_DIRECTORY,
    .handlers =
        {
            VNODE_DIR_HANDLERS(),
            [IRP_MJ_LOOKUP] = procfs_root_lookup,
            [IRP_MJ_READ] = procfs_root_read,
        },
};

static status_t procfs_clone_open(irp_t* irp);

static vnode_class_t cloneClass = {
    .name = "procfs clone",
    .type = FILE_TYPE_SYSTEM,
    .handlers =
        {
            VNODE_HANDLERS(),
            [IRP_MJ_OPEN] = procfs_clone_open,
        },
};

static filesystem_t procfs = {
    .name = PROCFS_NAME,
    .clone = &cloneClass,
};

static status_t procfs_clone_open(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    file_t* file = frame->file;
    if (file == NULL)
    {
        return ERR(FS, EXPECT_FILE);
    }

    filesystem_unregister(&procfs);

    return file_redirect(file, root);
}

void procfs_init(void)
{
    vnode_t* vnode = vnode_new(volume_new(), &rootClass, 0);
    if (vnode == NULL)
    {
        panic(NULL, "Failed to create procfs root vnode");
    }
    UNREF_DEFER(vnode);

    root = dentry_new(NULL, NULL, 0);
    if (root == NULL)
    {
        panic(NULL, "Failed to create procfs root dentry");
    }

    dentry_make_positive(root, vnode);

    status_t status = filesystem_register(&procfs);
    if (IS_ERR(status))
    {
        panic(NULL, "Failed to register procfs filesystem %Y", status);
    }
}