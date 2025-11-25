#include <kernel/cpu/ipi.h>
#include <kernel/init/init.h>

#include <kernel/acpi/acpi.h>
#include <kernel/acpi/aml/aml.h>
#include <kernel/acpi/devices.h>
#include <kernel/acpi/tables.h>
#include <kernel/cpu/cpu.h>
#include <kernel/cpu/gdt.h>
#include <kernel/cpu/idt.h>
#include <kernel/cpu/irq.h>
#include <kernel/cpu/syscalls.h>
#include <kernel/drivers/gop.h>
#include <kernel/drivers/pic.h>
#include <kernel/fs/ramfs.h>
#include <kernel/fs/sysfs.h>
#include <kernel/fs/vfs.h>
#include <kernel/log/log.h>
#include <kernel/log/log_file.h>
#include <kernel/log/panic.h>
#include <kernel/mem/pmm.h>
#include <kernel/mem/vmm.h>
#include <kernel/module/module.h>
#include <kernel/module/symbol.h>
#include <kernel/proc/process.h>
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

void init_early(const boot_info_t* bootInfo)
{
    gdt_init();
    idt_init();
    irq_init();

    cpu_init_early(&bootstrapCpu);
    assert(bootstrapCpu.id == CPU_ID_BOOTSTRAP);

    log_init(&bootInfo->gop);

    pmm_init(&bootInfo->memory.map);
    vmm_init(&bootInfo->memory, &bootInfo->gop, &bootInfo->kernel);

    _std_init();
    
    module_init_fake_kernel_module(&bootInfo->kernel);

    acpi_tables_init(bootInfo->rsdp);

    cpu_init(&bootstrapCpu);

    LOG_INFO("early init done, jumping to boot thread\n");
    thread_t* bootThread = thread_new(process_get_kernel());
    if (bootThread == NULL)
    {
        panic(NULL, "Failed to create boot thread");
    }

    bootThread->frame.rdi = (uintptr_t)bootInfo;
    bootThread->frame.rip = (uintptr_t)kmain;
    bootThread->frame.rbp = bootThread->kernelStack.top;
    bootThread->frame.rsp = bootThread->kernelStack.top;
    bootThread->frame.cs = GDT_CS_RING0;
    bootThread->frame.ss = GDT_SS_RING0;
    bootThread->frame.rflags = RFLAGS_ALWAYS_SET | RFLAGS_INTERRUPT_ENABLE;

    sched_start(bootThread);
}

// We delay freeing bootloader data as we are still using it during initialization.
static void init_free_loader_data(const boot_memory_map_t* map)
{
    // The memory map will be stored in the data we are freeing so we copy it first.
    boot_memory_map_t volatile mapCopy = *map;
    uint64_t descriptorsSize = map->descSize * map->length;
    EFI_MEMORY_DESCRIPTOR* volatile descriptorsCopy = malloc(descriptorsSize);
    if (descriptorsCopy == NULL)
    {
        panic(NULL, "Failed to allocate memory for boot memory map copy");
    }
    memcpy_s(descriptorsCopy, descriptorsSize, map->descriptors, descriptorsSize);
    mapCopy.descriptors = (EFI_MEMORY_DESCRIPTOR* const)descriptorsCopy;

    for (uint64_t i = 0; i < mapCopy.length; i++)
    {
        const EFI_MEMORY_DESCRIPTOR* desc = BOOT_MEMORY_MAP_GET_DESCRIPTOR(&mapCopy, i);

        if (desc->Type == EfiLoaderData)
        {
            LOG_INFO("free boot memory [0x%016lx-0x%016lx]\n", desc->VirtualStart,
                (uintptr_t)desc->VirtualStart + (desc->NumberOfPages * PAGE_SIZE));
#ifndef NDEBUG
            // Clear the memory to deliberately cause corruption if the memory is actually being used.
            memset((void*)desc->VirtualStart, 0xCC, desc->NumberOfPages * PAGE_SIZE);
#endif
            pmm_free_region((void*)desc->VirtualStart, desc->NumberOfPages);
        }
    }

    free(descriptorsCopy);
}

static void init_finalize(const boot_info_t* bootInfo)
{
    vmm_map_bootloader_lower_half();

    pic_disable();

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

    process_reaper_init();

    gop_init(&bootInfo->gop);
    perf_init();

    syscall_table_init();

    init_free_loader_data(&bootInfo->memory.map);
    vmm_unmap_bootloader_lower_half();

    if (module_device_attach("LOAD_ON_BOOT", "LOAD_ON_BOOT", MODULE_LOAD_ALL) == ERR)
    {
        panic(NULL, "Failed to load modules with LOAD_ON_BOOT");
    }

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
    const char* argv[] = {"/bin/init", NULL};
    thread_t* initThread = loader_spawn(argv, NULL, PRIORITY_MAX_USER - 2, SPAWN_DEFAULT);
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
    if (file_table_set(&initThread->process->fileTable, STDOUT_FILENO, klog) == ERR)
    {
        panic(NULL, "Failed to set klog as stdout for init process");
    }
    ref_dec(klog);

    sched_push(initThread, NULL);
}

void kmain(const boot_info_t* bootInfo)
{
    LOG_DEBUG("kmain entered\n");

    init_finalize(bootInfo);

    init_process_spawn();

    LOG_INFO("done with boot thread\n");
    sched_thread_exit();
}
