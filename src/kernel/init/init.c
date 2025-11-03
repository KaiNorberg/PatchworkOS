#include <kernel/init/init.h>

#include <kernel/acpi/acpi.h>
#include <kernel/acpi/aml/aml.h>
#include <kernel/acpi/devices.h>
#include <kernel/acpi/tables.h>
#include <kernel/cpu/gdt.h>
#include <kernel/cpu/idt.h>
#include <kernel/cpu/smp.h>
#include <kernel/cpu/syscalls.h>
#include <kernel/drivers/const.h>
#include <kernel/drivers/gop.h>
#include <kernel/drivers/ps2/ps2.h>
#include <kernel/fs/ramfs.h>
#include <kernel/fs/sysfs.h>
#include <kernel/fs/vfs.h>
#include <kernel/ipc/pipe.h>
#include <kernel/ipc/shmem.h>
#include <kernel/log/log.h>
#include <kernel/log/log_file.h>
#include <kernel/log/panic.h>
#include <kernel/mem/pmm.h>
#include <kernel/mem/vmm.h>
#include <kernel/module/module.h>
#include <kernel/module/symbol.h>
#include <kernel/net/net.h>
#include <kernel/proc/process.h>
#include <kernel/sched/loader.h>
#include <kernel/sched/sched.h>
#include <kernel/sched/timer.h>
#include <kernel/sched/wait.h>

#include <boot/boot_info.h>

#include <libstd/_internal/init.h>

#include <stdlib.h>
#include <strings.h>

void init_early(const boot_info_t* bootInfo)
{
    gdt_init();
    idt_init();

    smp_bootstrap_init_early();

    log_init(&bootInfo->gop);

    pmm_init(&bootInfo->memory.map);
    vmm_init(&bootInfo->memory, &bootInfo->gop, &bootInfo->kernel);

    _std_init();

    symbol_load_kernel_symbols(&bootInfo->kernel);

    acpi_tables_init(bootInfo->rsdp);

    smp_bootstrap_init();

    LOG_DEBUG("early init done, jumping to boot thread\n");
    thread_t* bootThread = thread_get_boot();
    assert(bootThread != NULL);
    bootThread->frame.rdi = (uintptr_t)bootInfo;
    bootThread->frame.rip = (uintptr_t)kmain;
    bootThread->frame.rbp = bootThread->kernelStack.top;
    bootThread->frame.rsp = bootThread->kernelStack.top;
    bootThread->frame.cs = GDT_CS_RING0;
    bootThread->frame.ss = GDT_SS_RING0;
    bootThread->frame.rflags = RFLAGS_ALWAYS_SET | RFLAGS_INTERRUPT_ENABLE;
    atomic_store(&bootThread->state, THREAD_RUNNING);
    bootThread->sched.deadline = CLOCKS_NEVER;
    smp_self_unsafe()->sched.runThread = bootThread;

    // This will trigger a page fault. But thats intended as we use page faults to dynamically grow the
    // threads kernel stack and the stack starts out unmapped.
    thread_jump(bootThread);
}

// We delay freeing bootloader data as we are still using it during initalization.
static void init_free_loader_data(const boot_memory_map_t* map)
{
    // The memory map will be stored in the data we are freeing so we copy it first.
    boot_memory_map_t volatile mapCopy = *map;
    EFI_MEMORY_DESCRIPTOR* volatile descriptorsCopy = malloc(mapCopy.descSize * mapCopy.length);
    if (descriptorsCopy == NULL)
    {
        panic(NULL, "Failed to allocate memory for boot memory map copy");
    }
    memcpy(descriptorsCopy, map->descriptors, mapCopy.descSize * mapCopy.length);
    mapCopy.descriptors = (EFI_MEMORY_DESCRIPTOR* const)descriptorsCopy;

    for (uint64_t i = 0; i < mapCopy.length; i++)
    {
        const EFI_MEMORY_DESCRIPTOR* desc = BOOT_MEMORY_MAP_GET_DESCRIPTOR(&mapCopy, i);

        if (desc->Type == EfiLoaderData)
        {
            LOG_INFO("free boot memory [0x%016lx-0x%016lx]\n", desc->VirtualStart,
                ((uintptr_t)desc->VirtualStart) + desc->NumberOfPages * PAGE_SIZE);
#ifndef NDEBUG
            // Clear the memory to deliberatly cause corruption if the memory is actually being used.
            memset((void*)desc->VirtualStart, 0xCC, desc->NumberOfPages * PAGE_SIZE);
#endif
            pmm_free_region((void*)desc->VirtualStart, desc->NumberOfPages);
        }
    }

    free(descriptorsCopy);
}

static void init_finalize(const boot_info_t* bootInfo)
{
    thread_t* bootThread = thread_get_boot();
    assert(bootThread != NULL);

    vmm_map_bootloader_lower_half(bootThread);

    vfs_init();
    ramfs_init(&bootInfo->disk);
    sysfs_init();

    aml_init();
    acpi_devices_init();
    acpi_reclaim_memory(&bootInfo->memory.map);

    acpi_tables_expose();
    aml_namespace_expose();

    log_file_expose();
    process_procfs_init();

    const_init();
    ps2_init();
    net_init();
    pipe_init();
    shmem_init();
    gop_init(&bootInfo->gop);
    perf_init();

    smp_others_init();

    syscall_table_init();

    init_free_loader_data(&bootInfo->memory.map);
    vmm_unmap_bootloader_lower_half(bootThread);

    if (module_load("/kernel/modules/helloworld") == ERR)
    {
        LOG_WARN("Failed to load hello world module (%s)\n", strerror(errno));
    }

    LOG_INFO("kernel initalized using %llu kb of memory\n", pmm_used_amount() * PAGE_SIZE / 1024);
}

static inline void init_process_spawn(void)
{
    LOG_INFO("spawning init process\n");
    const char* argv[] = {"/bin/init", NULL};
    thread_t* initThread = loader_spawn(argv, PRIORITY_MAX_USER - 2, NULL);
    if (initThread == NULL)
    {
        panic(NULL, "Failed to spawn init process");
    }

    // Set klog as stdout for init process
    file_t* klog = vfs_open(PATHNAME("/dev/klog"), initThread->process);
    if (klog == NULL)
    {
        panic(NULL, "Failed to open klog");
    }
    if (vfs_ctx_set_fd(&initThread->process->vfsCtx, STDOUT_FILENO, klog) == ERR)
    {
        panic(NULL, "Failed to set klog as stdout for init process");
    }
    ref_dec(klog);

    sched_push(initThread, NULL);
}

void kmain(const boot_info_t* bootInfo)
{
    // The stack pointer is expected to be somewhere near the top.
    assert(rsp_read() > VMM_KERNEL_STACKS_MAX - 2 * PAGE_SIZE);

    LOG_DEBUG("kmain entered\n");

    init_finalize(bootInfo);

    asm volatile("sti");

    init_process_spawn();

    LOG_INFO("done with boot thread\n");
    sched_done_with_boot_thread();
}
