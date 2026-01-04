#include <kernel/fs/namespace.h>
#include <kernel/init/init.h>

#include <kernel/cpu/cpu.h>
#include <kernel/cpu/gdt.h>
#include <kernel/cpu/idt.h>
#include <kernel/cpu/ipi.h>
#include <kernel/cpu/irq.h>
#include <kernel/cpu/syscall.h>
#include <kernel/drivers/pic.h>
#include <kernel/fs/procfs.h>
#include <kernel/fs/devfs.h>
#include <kernel/fs/tmpfs.h>
#include <kernel/fs/netfs.h>
#include <kernel/fs/vfs.h>
#include <kernel/init/boot_info.h>
#include <kernel/log/log.h>
#include <kernel/log/log_file.h>
#include <kernel/log/panic.h>
#include <kernel/mem/pmm.h>
#include <kernel/mem/vmm.h>
#include <kernel/module/module.h>
#include <kernel/module/symbol.h>
#include <kernel/proc/group.h>
#include <kernel/proc/process.h>
#include <kernel/proc/reaper.h>
#include <kernel/sched/loader.h>
#include <kernel/sched/sched.h>
#include <kernel/sched/thread.h>
#include <kernel/sched/timer.h>
#include <kernel/sched/wait.h>

#include <boot/boot_info.h>

#include <libstd/_internal/init.h>

#include <stdlib.h>
#include <string.h>
#include <strings.h>

static cpu_t bootstrapCpu ALIGNED(PAGE_SIZE) = {0};

void init_early(void)
{
    gdt_init();
    idt_init();
    irq_init();

    cpu_init_early(&bootstrapCpu);
    assert(bootstrapCpu.id == CPU_ID_BOOTSTRAP);

    log_init();

    pmm_init();
    vmm_init();

    boot_info_to_higher_half();

    vmm_kernel_space_load();

    _std_init();

    module_init_fake_kernel_module();

    cpu_init(&bootstrapCpu);

    LOG_INFO("early init done, jumping to boot thread\n");
    thread_t* bootThread = thread_new(process_get_kernel());
    if (bootThread == NULL)
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
}

static void init_finalize(void)
{
    pic_disable();

    tmpfs_init();
    devfs_init();
    procfs_init();
    netfs_init();

    log_file_expose();

    reaper_init();

    perf_init();

    syscall_table_init();

    boot_info_t* bootInfo = boot_info_get();

    if (bootInfo->gop.virtAddr != NULL)
    {
        if (module_device_attach("BOOT_GOP", "BOOT_GOP", MODULE_LOAD_ALL) == ERR)
        {
            panic(NULL, "Failed to load modules with BOOT_GOP");
        }
    }
    else
    {
        LOG_WARN("no GOP provided by bootloader\n");
    }

    if (bootInfo->rsdp != NULL)
    {
        if (module_device_attach("BOOT_RSDP", "BOOT_RSDP", MODULE_LOAD_ALL) == ERR)
        {
            panic(NULL, "Failed to load modules with BOOT_RSDP");
        }
    }
    else
    {
        LOG_WARN("no RSDP provided by bootloader\n");
    }

    if (module_device_attach("BOOT_ALWAYS", "BOOT_ALWAYS", MODULE_LOAD_ALL) == ERR)
    {
        panic(NULL, "Failed to load modules with BOOT_ALWAYS");
    }

    boot_info_free();

    if (timer_source_amount() == 0)
    {
        panic(NULL, "No timer source registered, most likely no timer sources with a provided driver was found");
    }
    if (irq_chip_amount() == 0)
    {
        panic(NULL, "No IRQ chip registered, most likely no IRQ chips with a provided driver was found");
    }
    if (ipi_chip_amount() == 0)
    {
        panic(NULL, "No IPI chip registered, most likely no IPI chips with a provided driver was found");
    }

    LOG_INFO("kernel initalized using %llu kb of memory\n", pmm_used_amount() * PAGE_SIZE / 1024);
}

static inline void init_process_spawn(void)
{
    LOG_INFO("spawning init process\n");

    namespace_t* kernelNs = process_get_ns(process_get_kernel());
    if (kernelNs == NULL)
    {
        panic(NULL, "Failed to get kernel namespace");
    }
    UNREF_DEFER(kernelNs);

    namespace_t* rootNs = namespace_new(kernelNs);
    if (rootNs == NULL)
    {
        panic(NULL, "Failed to create root namespace");
    }
    UNREF_DEFER(rootNs);

    if (namespace_copy(rootNs, kernelNs) == ERR)
    {
        panic(NULL, "Failed to copy kernel namespace to root namespace");
    }

    process_t* initProcess = process_new(PRIORITY_MAX_USER, NULL, rootNs);
    if (initProcess == NULL)
    {
        panic(NULL, "Failed to create init process");
    }
    UNREF_DEFER(initProcess);

    thread_t* initThread = thread_new(initProcess);
    if (initThread == NULL)
    {
        panic(NULL, "Failed to create init thread");
    }

    char* argv[] = {"/sbin/init", NULL};
    if (process_set_cmdline(initProcess, argv, 1) == ERR)
    {
        panic(NULL, "Failed to set init process cmdline");
    }

    // Calls loader_exec();
    initThread->frame.rip = (uintptr_t)loader_exec;
    initThread->frame.cs = GDT_CS_RING0;
    initThread->frame.ss = GDT_SS_RING0;
    initThread->frame.rsp = initThread->kernelStack.top;
    initThread->frame.rflags = RFLAGS_INTERRUPT_ENABLE | RFLAGS_ALWAYS_SET;

    sched_submit(initThread);
}

void kmain(void)
{
    LOG_DEBUG("kmain entered\n");

    init_finalize();

    init_process_spawn();

    LOG_INFO("done with boot thread\n");
    sched_thread_exit();
}