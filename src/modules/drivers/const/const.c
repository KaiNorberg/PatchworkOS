#include <kernel/fs/sysfs.h>
#include <kernel/fs/vfs.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/mem/vmm.h>
#include <kernel/module/module.h>
#include <kernel/proc/process.h>
#include <kernel/sched/sched.h>

#include <stdint.h>
#include <string.h>

/**
 * @brief Constant devices
 * @defgroup modules_drivers_const Constant Devices
 * @ingroup modules_drivers
 *
 * This module provides the constant devices which provide user space
with its primary means of allocating memory and obtaining constant data.
 *
 * The constant devices are exposed under the `/dev` directory:
 * - `/dev/one`: A readable and mappable file that returns bytes with all bits set to 1.
 * - `/dev/zero`: A readable and mappable file that returns bytes with all bits set to 0.
 * - `/dev/null`: A readable and writable file that discards all written data and returns EOF on read.
 *
 * @{
 */

static dentry_t* oneFile;
static dentry_t* zeroFile;
static dentry_t* nullFile;

static uint64_t const_one_read(file_t* file, void* buffer, uint64_t count, uint64_t* offset)
{
    (void)file; // Unused

    memset(buffer, -1, count);
    *offset += count;
    return count;
}

static void* const_one_mmap(file_t* file, void* addr, uint64_t length, uint64_t* offset, pml_flags_t flags)
{
    (void)file;   // Unused
    (void)offset; // Unused

    addr = vmm_alloc(&sched_process()->space, addr, length, flags, VMM_ALLOC_NONE);
    if (addr == NULL)
    {
        return NULL;
    }

    memset(addr, -1, length);
    return addr;
}

static file_ops_t oneOps = {
    .read = const_one_read,
    .mmap = const_one_mmap,
};

static uint64_t const_zero_read(file_t* file, void* buffer, uint64_t count, uint64_t* offset)
{
    (void)file; // Unused

    memset(buffer, 0, count);
    *offset += count;
    return count;
}

static void* const_zero_mmap(file_t* file, void* addr, uint64_t length, uint64_t* offset, pml_flags_t flags)
{
    (void)file;   // Unused
    (void)offset; // Unused

    addr = vmm_alloc(&sched_process()->space, addr, length, flags, VMM_ALLOC_NONE);
    if (addr == NULL)
    {
        return NULL;
    }

    memset(addr, 0, length);
    return addr;
}

static file_ops_t zeroOps = {
    .read = const_zero_read,
    .mmap = const_zero_mmap,
};

static uint64_t const_null_read(file_t* file, void* buffer, uint64_t count, uint64_t* offset)
{
    (void)file;   // Unused
    (void)buffer; // Unused

    *offset += count;
    return 0;
}

static uint64_t const_null_write(file_t* file, const void* buffer, uint64_t count, uint64_t* offset)
{
    (void)file;   // Unused
    (void)buffer; // Unused

    *offset += count;
    return count;
}

static file_ops_t nullOps = {
    .read = const_null_read,
    .write = const_null_write,
};

static uint64_t const_init(void)
{
    oneFile = sysfs_file_new(NULL, "one", NULL, &oneOps, NULL);
    if (oneFile == NULL)
    {
        LOG_ERR("failed to init one file\n");
        return ERR;
    }

    zeroFile = sysfs_file_new(NULL, "zero", NULL, &zeroOps, NULL);
    if (zeroFile == NULL)
    {
        DEREF(oneFile);
        LOG_ERR("failed to init zero file\n");
        return ERR;
    }

    nullFile = sysfs_file_new(NULL, "null", NULL, &nullOps, NULL);
    if (nullFile == NULL)
    {
        DEREF(oneFile);
        DEREF(zeroFile);
        LOG_ERR("failed to init null file\n");
        return ERR;
    }

    return 0;
}

static void const_deinit(void)
{
    DEREF(oneFile);
    DEREF(zeroFile);
    DEREF(nullFile);
}

/** @} */

uint64_t _module_procedure(const module_event_t* event)
{
    switch (event->type)
    {
    case MODULE_EVENT_LOAD:
        const_init();
        break;
    case MODULE_EVENT_UNLOAD:
        const_deinit();
        break;
    default:
        break;
    }

    return 0;
}

MODULE_INFO("Const Driver", "Kai Norberg", "A constant device driver", OS_VERSION, "MIT", "LOAD_ON_BOOT");