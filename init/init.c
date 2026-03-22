#include <libtar/tar.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/comp.h>
#include <sys/io.h>
#include <sys/scon.h>

/**
 * @brief User space init process.
 * @defgroup init Init Process
 *
 * The init process is the first user space process started by the kernel. It is responsible for initializing user space
 * and spawning other processes.
 *
 * ## Initial Capabilities
 *
 * The init process is handed one capability, within `FDROOT` and `FDCWD` will be the root of the sysfs filesystem which
 * it will use to bootstrap the system.
 *
 * Additionally, the klog file can be found in `FDOUT` however it could already access this file within sysfs so we do
 * not consider it another capability merely more convenient incase of early errors in the init process.
 *
 */

static void print_dir(fd_t dir, uint32_t depth)
{
    file_info_t info;
    status_t status = ioquery(dir, &info);
    if (IS_ERR(status))
    {
        printf("init: failed to query dir %Y\n", status);
        return;
    }

    if (info.type != FILE_TYPE_DIRECTORY)
    {
        return;
    }

    char contents[2000];
    size_t length;
    status = ioread(dir, IOBUF(contents, sizeof(contents)), 0, &length);
    if (IS_ERR(status))
    {
        printf("init: failed to read dir %Y\n", status);
        return;
    }

    const char* p = contents;
    const char* end = contents + length;

    while (p < end)
    {
        size_t len = strlen(p);
        if (len == 0 || p[0] == '.')
        {
            p += len + 1;
            continue;
        }

        fd_t child;
        status = iowalk(dir, dir, p, &child);
        if (IS_ERR(status))
        {
            printf("init: failed to walk to child (%s) %Y\n", p, status);
            return;
        }

        file_type_t type = 0;
        status = ioattr(child, FILE_GET_TYPE, &type);
        if (IS_ERR(status))
        {
            iodrop(child);
            printf("init: failed to get child type %Y\n", status);
            return;
        }

        for (uint32_t i = 0; i < depth; i++)
        {
            printf("  ");
        }
        printf("%s\n", p);

        if (type == FILE_TYPE_DIRECTORY && strcmp(p, "clone") != 0)
        {
            print_dir(child, depth + 1);
        }

        iodrop(child);

        p += len + 1;
    }
}

static status_t initrd_load(void)
{
    char* data;
    size_t size;
    status_t status = ioloadp(FDCWD, FDROOT, "/sys/initrd:r", &data, &size);
    if (IS_ERR(status))
    {
        return status;
    }

    tar_t tar;
    tar_init(&tar, data, size);

    tar_iter_t iter;
    tar_iter_init(&iter, &tar);

    tar_entry_t entry;
    while (true)
    {
        status = tar_next(&iter, &entry);
        if (IS_ERR(status))
        {
            free(data);
            return status;
        }

        if (IS_CODE(status, EOF))
        {
            break;
        }

        switch (entry.type)
        {
        case TAR_TYPE_DIR:
        {
            fd_t dir;
            status = iowalk(FDCWD, FDROOT, IOFMT("/%s:pd", entry.name), &dir);
            if (IS_ERR(status))
            {
                printf("init: failed to create dir %s: %Y\n", entry.name, status);
                continue;
            }
            iodrop(dir);
        }
        break;
        case TAR_TYPE_REGULAR:
        {
            status = iowritep(FDCWD, FDROOT, IOFMT("/%s:pc", entry.name), IOBUF(entry.data, entry.size), 0, NULL);
            if (IS_ERR(status))
            {
                printf("init: failed to create regular file %s %Y\n", entry.name, status);
                continue;
            }
        }
        break;
        case TAR_TYPE_SYMLINK:
        {
            fd_t symlink;
            status = iowalk(FDCWD, FDROOT, IOFMT("/%s:ps?%.*s", entry.name, entry.size, entry.data), &symlink);
            if (IS_ERR(status))
            {
                printf("init: failed to create symlink %s: %Y\n", entry.name, status);
                continue;
            }
            iodrop(symlink);
        }
        break;
        default:
            printf("init: unknown tar entry type %u for %s\n", entry.type, entry.name);
            continue;
        }

    }

    free(data);
    return OK;
}

int main(void)
{
    printf("init: init process started\n");

    printf("init: binding sysfs to /sys within tmpfs\n");
    fd_t tmpfs;
    status_t status = iowalk(FDCWD, FDROOT, "/fs/tmpfs/clone:rwx", &tmpfs);
    if (IS_ERR(status))
    {
        printf("init: failed to walk to tmpfs %Y\n", status);
        return EXIT_FAILURE;
    }

    fd_t sysdir;
    status = iowalk(tmpfs, tmpfs, "/sys:rwxd", &sysdir);
    if (IS_ERR(status))
    {
        iodrop(tmpfs);
        printf("init: failed to walk to /sys %Y\n", status);
        return EXIT_FAILURE;
    }

    status = fdbind(tmpfs, sysdir, FDROOT);
    if (IS_ERR(status))
    {
        iodrop(tmpfs);
        iodrop(sysdir);
        printf("init: failed to bind /sys to root %Y\n", status);
        return EXIT_FAILURE;
    }
    iodrop(sysdir);

    printf("init: binding devfs to /dev within tmpfs\n");
    fd_t devfs;
    status = iowalk(FDROOT, FDROOT, "/fs/devfs/clone:rwx", &devfs);
    if (IS_ERR(status))
    {
        iodrop(tmpfs);
        printf("init: failed to walk to devfs %Y\n", status);
        return EXIT_FAILURE;
    }

    fd_t devdir;
    status = iowalk(tmpfs, tmpfs, "/dev:rwxd", &devdir);
    if (IS_ERR(status))
    {
        iodrop(tmpfs);
        iodrop(devfs);
        printf("init: failed to walk to /dev %Y\n", status);
        return EXIT_FAILURE;
    }

    status = fdbind(tmpfs, devdir, devfs);
    if (IS_ERR(status))
    {
        iodrop(tmpfs);
        iodrop(devfs);
        iodrop(devdir);
        printf("init: failed to bind /dev %Y\n", status);
        return EXIT_FAILURE;
    }

    iodrop(devfs);
    iodrop(devdir);

    printf("init: binding procfs to /proc within tmpfs\n");
    fd_t procfs;
    status = iowalk(FDROOT, FDROOT, "/fs/procfs/clone:rwx", &procfs);
    if (IS_ERR(status))
    {
        iodrop(tmpfs);
        printf("init: failed to walk to procfs %Y\n", status);
        return EXIT_FAILURE;
    }

    fd_t procdir;
    status = iowalk(tmpfs, tmpfs, "/proc:rwxd", &procdir);
    if (IS_ERR(status))
    {
        iodrop(tmpfs);
        iodrop(procfs);
        printf("init: failed to walk to /proc %Y\n", status);
        return EXIT_FAILURE;
    }

    status = fdbind(tmpfs, procdir, procfs);
    if (IS_ERR(status))
    {
        iodrop(tmpfs);
        iodrop(procfs);
        iodrop(procdir);
        printf("init: failed to bind /proc %Y\n", status);
        return EXIT_FAILURE;
    }

    iodrop(procfs);
    iodrop(procdir);

    printf("init: setting new root and cwd\n");
    fddup(tmpfs, (fd_t[]){FDROOT});
    fddup(tmpfs, (fd_t[]){FDCWD});
    iodrop(tmpfs);

    printf("init: loading initrd\n");
    status = initrd_load();
    if (IS_ERR(status))
    {
        iodrop(tmpfs);
        printf("init: failed to load initrd %Y\n", status);
        return EXIT_FAILURE;
    }

    print_dir(FDROOT, 0);

    comp_options_t opts;
    opts.stdin = FDIN;
    opts.stdout = FDOUT;
    opts.stderr = FDERR;
    status = comp_launch("test", "1.0.0", &opts);
    if (IS_ERR(status))
    {
        printf("init: failed to launch component %Y\n", status);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
