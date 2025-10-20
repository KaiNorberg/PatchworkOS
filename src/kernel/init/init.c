#include "init.h"

#include "acpi/acpi.h"
#include "acpi/aml/aml.h"
#include "acpi/devices.h"
#include "acpi/tables.h"
#include "cpu/gdt.h"
#include "cpu/idt.h"
#include "cpu/smp.h"
#include "cpu/syscalls.h"
#include "drivers/const.h"
#include "drivers/fb/gop.h"
#include "drivers/ps2/ps2.h"
#include "fs/ramfs.h"
#include "fs/sysfs.h"
#include "fs/vfs.h"
#include "gnu-efi/inc/efidef.h"
#include "ipc/pipe.h"
#include "ipc/shmem.h"
#include "log/log.h"
#include "log/log_file.h"
#include "log/panic.h"
#include "mem/heap.h"
#include "mem/pmm.h"
#include "mem/vmm.h"
#include "net/net.h"
#include "proc/process.h"
#include "sched/loader.h"
#include "sched/sched.h"
#include "sched/timer.h"
#include "sched/wait.h"

#include <boot/boot_info.h>
#include <libstd/_internal/init.h>
#include <strings.h>

void init_early(const boot_info_t* bootInfo)
{
    gdt_init();
    idt_init();

    smp_early_bootstrap_init();

    log_init(&bootInfo->gop);

    pmm_init(&bootInfo->memory.map);
    vmm_init(&bootInfo->memory, &bootInfo->gop, &bootInfo->kernel);
    heap_init();

    acpi_tables_init(bootInfo->rsdp);

    smp_bootstrap_init();

    timer_init();
    sched_init();
    wait_init();

    panic_symbols_init(&bootInfo->kernel);

    LOG_DEBUG("early init done, jumping to boot thread\n");
    thread_t* bootThread = thread_get_boot();
    assert(bootThread != NULL);
    bootThread->frame.rdi = (uintptr_t)bootInfo;
    bootThread->frame.rip = (uintptr_t)kmain;
    bootThread->frame.rsp = bootThread->kernelStack.top;
    bootThread->frame.cs = GDT_CS_RING0;
    bootThread->frame.ss = GDT_SS_RING0;
    bootThread->frame.rflags = RFLAGS_ALWAYS_SET;
    // This will trigger a page fault. But thats intended as we use page faults to dynamically grow the
    // threads kernel stack and the stack starts out unmapped.
    thread_jump(bootThread);
}

static void init_free_loader_data(const boot_memory_map_t* map)
{
    for (uint64_t i = 0; i < map->length; i++)
    {
        const EFI_MEMORY_DESCRIPTOR* desc = BOOT_MEMORY_MAP_GET_DESCRIPTOR(map, i);

        if (desc->Type == EfiLoaderData)
        {
            pmm_free_region((void*)desc->VirtualStart, desc->NumberOfPages);
            LOG_INFO("free boot memory [0x%016lx-0x%016lx]\n", desc->VirtualStart,
                ((uintptr_t)desc->VirtualStart) + desc->NumberOfPages * PAGE_SIZE);
        }
    }
}

static void init_finalize(const boot_info_t* bootInfo)
{
    thread_t* bootThread = thread_get_boot();
    assert(bootThread != NULL);

    vmm_map_bootloader_lower_half(bootThread);

    _std_init();

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
    statistics_init();

    smp_others_init();

    syscall_table_init();

    init_free_loader_data(&bootInfo->memory.map);
    vmm_unmap_bootloader_lower_half(bootThread);

    LOG_INFO("kernel initalized using %llu kb of memory\n", pmm_reserved_amount() * PAGE_SIZE / 1024);
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
