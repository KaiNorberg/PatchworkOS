#include <kernel/fs/devfs.h>
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
 * This module provides the constant devices which provide user space with its primary means of allocating memory and
 * obtaining constant data.
 *
 * The constant devices are exposed under the `/dev/const/` directory:
 * - `/dev/const/one`: A readable and mappable file that returns bytes with all bits set to 1.
 * - `/dev/const/zero`: A readable and mappable file that returns bytes with all bits set to 0.
 * - `/dev/const/null`: A readable and writable file that discards all written data and returns EOF on read.
 *
 * @{
 */

static dentry_t* constDir;
static dentry_t* oneFile;
static dentry_t* zeroFile;
static dentry_t* nullFile;

static uint64_t const_one_read(file_t* file, void* buffer, size_t count, size_t* offset)
{
    UNUSED(file);

    memset(buffer, -1, count);
    *offset += count;
    return count;
}

static void* const_one_mmap(file_t* file, void* addr, size_t length, size_t* offset, pml_flags_t flags)
{
    UNUSED(file); // Unused
    UNUSED(offset);

    addr = vmm_alloc(&process_current()->space, addr, length, PAGE_SIZE, flags, VMM_ALLOC_OVERWRITE);
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

static uint64_t const_zero_read(file_t* file, void* buffer, size_t count, size_t* offset)
{
    UNUSED(file);

    memset(buffer, 0, count);
    *offset += count;
    return count;
}

static void* const_zero_mmap(file_t* file, void* addr, size_t length, size_t* offset, pml_flags_t flags)
{
    UNUSED(file); // Unused
    UNUSED(offset);

    addr = vmm_alloc(&process_current()->space, addr, length, PAGE_SIZE, flags, VMM_ALLOC_OVERWRITE);
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

static uint64_t const_null_read(file_t* file, void* buffer, size_t count, size_t* offset)
{
    UNUSED(file); // Unused
    UNUSED(buffer);

    *offset += count;
    return 0;
}

static uint64_t const_null_write(file_t* file, const void* buffer, size_t count, size_t* offset)
{
    UNUSED(file); // Unused
    UNUSED(buffer);

    *offset += count;
    return count;
}

static file_ops_t nullOps = {
    .read = const_null_read,
    .write = const_null_write,
};

static uint64_t const_init(void)
{
    constDir = devfs_dir_new(NULL, "const", NULL, NULL);
    if (constDir == NULL)
    {
        LOG_ERR("failed to init const directory\n");
        return ERR;
    }

    oneFile = devfs_file_new(constDir, "one", NULL, &oneOps, NULL);
    if (oneFile == NULL)
    {
        UNREF(constDir);
        LOG_ERR("failed to init one file\n");
        return ERR;
    }

    zeroFile = devfs_file_new(constDir, "zero", NULL, &zeroOps, NULL);
    if (zeroFile == NULL)
    {
        UNREF(constDir);
        UNREF(oneFile);
        LOG_ERR("failed to init zero file\n");
        return ERR;
    }

    nullFile = devfs_file_new(constDir, "null", NULL, &nullOps, NULL);
    if (nullFile == NULL)
    {
        UNREF(constDir);
        UNREF(oneFile);
        UNREF(zeroFile);
        LOG_ERR("failed to init null file\n");
        return ERR;
    }

    return 0;
}

static void const_deinit(void)
{
    UNREF(constDir);
    UNREF(oneFile);
    UNREF(zeroFile);
    UNREF(nullFile);
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

MODULE_INFO("Const Driver", "Kai Norberg", "A constant device driver", OS_VERSION, "MIT", "BOOT_ALWAYS");