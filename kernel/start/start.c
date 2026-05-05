#include <kernel/cpu/cpu.h>
#include <kernel/cpu/gdt.h>
#include <kernel/cpu/idt.h>
#include <kernel/cpu/ipi.h>
#include <kernel/cpu/irq.h>
#include <kernel/cpu/syscall.h>
#include <kernel/drivers/abstract/fb.h>
#include <kernel/drivers/abstract/kbd.h>
#include <kernel/drivers/abstract/mouse.h>
#include <kernel/drivers/announce.h>
#include <kernel/drivers/const.h>
#include <kernel/drivers/pic.h>
#include <kernel/fs/concatfs.h>
#include <kernel/fs/devfs.h>
#include <kernel/fs/filesystem.h>
#include <kernel/fs/path.h>
#include <kernel/fs/procfs.h>
#include <kernel/fs/sysfs.h>
#include <kernel/fs/tmpfs.h>
#include <kernel/fs/vfs.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/log/screen.h>
#include <kernel/mem/pmm.h>
#include <kernel/mem/vmm.h>
#include <kernel/module/module.h>
#include <kernel/module/symbol.h>
#include <kernel/proc/job.h>
#include <kernel/proc/process.h>
#include <kernel/sched/sched.h>
#include <kernel/sched/thread.h>
#include <kernel/sched/timer.h>
#include <kernel/sched/wait.h>
#include <kernel/start/boot_info.h>
#include <kernel/start/start.h>

#include <boot/boot_info.h>

#include <_libc/init.h>

#ifdef _TESTING_
#include <kernel/utils/test.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <strings.h>

static cpu_t bootstrapCpu ALIGNED(PAGE_SIZE) = {0};

void start_early(void)
{
    gdt_init();
    idt_init();
    irq_init();

    cpu_init(&bootstrapCpu);

    log_init();

    pmm_init();
    vmm_init();

    boot_info_to_higher_half();

    vmm_kernel_space_load();

    syscall_table_init();

    _std_init();

    PERCPU_INIT();

    module_init_fake_kernel_module();

    LOG_INFO("early init done, jumping to boot thread\n");
    thread_t* bootThread;
    status_t status = thread_new(&bootThread, process_get_kernel());
    if (IS_ERR(status))
    {
        panic(NULL, "Failed to create boot thread");
    }

    bootThread->frame.rip = (uintptr_t)kmain;
    bootThread->frame.rbp = bootThread->kernelStack.top;
    bootThread->frame.rsp = bootThread->kernelStack.top;
    bootThread->frame.cs = GDT_CS_RING0;
    bootThread->frame.ss = GDT_SS_RING0;
    bootThread->frame.rflags = RFLAGS_ALWAYS_SET | RFLAGS_INTERRUPT_ENABLE;

    sched_start(bootThread);
    panic(NULL, "sched_start returned unexpectedly");
}

static void start_finalize(void)
{
    pic_disable();

    sysfs_init();
    devfs_init();
    procfs_init();
    tmpfs_init();
    concatfs_init();

    fb_init();
    kbd_init();
    mouse_init();

    log_expose();
    module_expose();
    announce_init();
    boot_info_expose_initrd();

    perf_init();
    const_init();

    LOG_INFO("kernel initalized using %llu kb of memory\n", pmm_used_pages() * PAGE_SIZE / 1024);
}

static inline void start_init_process(void)
{
    LOG_INFO("spawning init process\n");

    job_t* job;
    status_t status = job_new(&job, NULL);
    if (IS_ERR(status))
    {
        panic(NULL, "Failed to create init job");
    }
    UNREF_DEFER(job);

    process_t* initProcess;
    status = process_new(&initProcess, PRIO_MAX_USER, job);
    if (IS_ERR(status))
    {
        panic(NULL, "Failed to create init process");
    }
    UNREF_DEFER(initProcess);

    file_t* sysfs;
    status = sysfs_root_file(&sysfs);
    if (IS_ERR(status))
    {
        panic(NULL, "Failed to get sysfs root file");
    }
    UNREF_DEFER(sysfs);

    fd_t fd = FDROOT;
    status = file_table_grab(&initProcess->files, sysfs, &fd);
    if (IS_ERR(status))
    {
        panic(NULL, "Failed to grab sysfs root file");
    }

    fd = FDCWD;
    status = file_table_grab(&initProcess->files, sysfs, &fd);
    if (IS_ERR(status))
    {
        panic(NULL, "Failed to grab sysfs root file");
    }

    dentry_t* klogDentry = log_dentry();
    if (klogDentry == NULL)
    {
        panic(NULL, "Failed to get klog dentry");
    }
    UNREF_DEFER(klogDentry);

    file_t* klog = file_new(klogDentry, sysfs->path.binding, PATH_MODE_ALL_PERMS);
    if (klog == NULL)
    {
        panic(NULL, "Failed to create klog file");
    }
    UNREF_DEFER(klog);

    fd = FDOUT;
    status = file_table_grab(&initProcess->files, klog, &fd);
    if (IS_ERR(status))
    {
        panic(NULL, "Failed to grab klog file");
    }

    thread_t* initThread;
    status = thread_new(&initThread, initProcess);
    if (IS_ERR(status))
    {
        panic(NULL, "Failed to create init thread");
    }

    boot_info_t* bootInfo = boot_info_get();
    if (bootInfo->init.buffer == NULL)
    {
        panic(NULL, "No init process loaded by bootloader");
    }

    void* buffer = (void*)PML_ENSURE_HIGHER_HALF(bootInfo->init.buffer);
    void* loadAddr = (void*)bootInfo->init.loadAddr;
    size_t size = bootInfo->init.size;

    status = vmm_alloc(&initProcess->space, &loadAddr, size, PAGE_SIZE, PML_USER | PML_WRITE | PML_PRESENT,
        VMM_ALLOC_OVERWRITE);
    if (IS_ERR(status))
    {
        panic(NULL, "Failed to allocate memory for init process");
    }

    status = space_copy_in(&initProcess->space, loadAddr, buffer, size);
    if (IS_ERR(status))
    {
        panic(NULL, "Failed to copy init process to user space");
    }

    memset(&initThread->frame, 0, sizeof(interrupt_frame_t));
    initThread->frame.rip = bootInfo->init.entry;
    initThread->frame.cs = GDT_CS_RING3;
    initThread->frame.ss = GDT_SS_RING3;
    initThread->frame.rflags = RFLAGS_INTERRUPT_ENABLE | RFLAGS_ALWAYS_SET;

    sched_submit(initThread);
}

void kmain(void)
{
    LOG_DEBUG("kmain entered\n");

    start_finalize();

#ifdef _TESTING_
    TEST_ALL();
#endif

    start_init_process();

    LOG_INFO("done with boot thread\n");
    sched_thread_exit();
}
