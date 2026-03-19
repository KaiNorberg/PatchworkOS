#include <stdio.h>
#include <stdlib.h>
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

static void print_scon(scon_ref_t list)
{
    scon_ref_t ref;
    SCON_FOR_EACH(ref, list)
    {
        if (scon_is_list(ref))
        {
            printf("(");
            print_scon(ref);
            printf(")");
        }
        else
        {
            printf("%.*s", (int)scon_atom_len(ref), scon_atom_str(ref));
        }
    }
}

int main(void)
{
    printf("init: init process started\n");

    printf("init: binding sysfs to /sys within ramfs\n");
    fd_t ramfs;
    status_t status = iowalk(FDCWD, FDROOT, "/fs/ramfs/clone:rwx", &ramfs);
    if (IS_ERR(status))
    {
        printf("init: failed to walk to ramfs %Y\n", status);
        return EXIT_FAILURE;
    }

    fd_t sysdir;
    status = iowalk(ramfs, ramfs, "/sys:rwxd", &sysdir);
    if (IS_ERR(status))
    {
        iodrop(ramfs);
        printf("init: failed to walk to /sys %Y\n", status);
        return EXIT_FAILURE;
    }

    status = fdbind(ramfs, sysdir, FDROOT);
    if (IS_ERR(status))
    {
        iodrop(ramfs);
        iodrop(sysdir);
        printf("init: failed to bind /sys to root %Y\n", status);
        return EXIT_FAILURE;
    }
    iodrop(sysdir);

    printf("init: binding devfs to /dev within ramfs\n");
    fd_t devfs;
    status = iowalk(FDROOT, FDROOT, "/fs/devfs/clone:rwx", &devfs);
    if (IS_ERR(status))
    {
        iodrop(ramfs);
        printf("init: failed to walk to devfs %Y\n", status);
        return EXIT_FAILURE;
    }

    fd_t devdir;
    status = iowalk(ramfs, ramfs, "/dev:rwxd", &devdir);
    if (IS_ERR(status))
    {
        iodrop(ramfs);
        iodrop(devfs);
        printf("init: failed to walk to /dev %Y\n", status);
        return EXIT_FAILURE;
    }

    status = fdbind(ramfs, devdir, devfs);
    if (IS_ERR(status))
    {
        iodrop(ramfs);
        iodrop(devfs);
        iodrop(devdir);
        printf("init: failed to bind /dev %Y\n", status);
        return EXIT_FAILURE;
    }

    iodrop(devfs);
    iodrop(devdir);

    printf("init: binding procfs to /proc within ramfs\n");
    fd_t procfs;
    status = iowalk(FDROOT, FDROOT, "/fs/procfs/clone:rwx", &procfs);
    if (IS_ERR(status))
    {
        iodrop(ramfs);
        printf("init: failed to walk to procfs %Y\n", status);
        return EXIT_FAILURE;
    }

    fd_t procdir;
    status = iowalk(ramfs, ramfs, "/proc:rwxd", &procdir);
    if (IS_ERR(status))
    {
        iodrop(ramfs);
        iodrop(procfs);
        printf("init: failed to walk to /proc %Y\n", status);
        return EXIT_FAILURE;
    }

    status = fdbind(ramfs, procdir, procfs);
    if (IS_ERR(status))
    {
        iodrop(ramfs);
        iodrop(procfs);
        iodrop(procdir);
        printf("init: failed to bind /proc %Y\n", status);
        return EXIT_FAILURE;
    }

    iodrop(procfs);
    iodrop(procdir);

    printf("init: setting new root and cwd\n");
    fddup(ramfs, (fd_t[]){FDROOT});
    fddup(ramfs, (fd_t[]){FDCWD});
    iodrop(ramfs);

    print_dir(FDROOT, 0);

    const char* testString = "(component"
                             "    (description \"An example component.\")"
                             "    (author Kai)"
                             "    (license MIT)"
                             "    (launch bin/example)"
                             "    (dependencies"
                             "        (libstd >= 1.0.0)"
                             "        )"
                             "    (capabilities"
                             "        fb"
                             "        kbd"
                             "        )"
                             ")";

    scon_t testLisc;
    status = scon_init(&testLisc, testString, strlen(testString));
    if (IS_ERR(status))
    {
        printf("init: failed to init scon %Y\n", status);
        printf(testLisc.error);
        return EXIT_FAILURE;
    }

    print_scon(scon_root(&testLisc));

    proc_fd_t fds[] = {
        {
            .parent = FDROOT,
            .child = FDROOT,
        },
        {
            .parent = FDCWD,
            .child = FDCWD,
        },
        {
            .parent = FDOUT,
            .child = FDOUT,
        },
    };
    status =
        proc_create(FDCWD, FDROOT, PROC_ARGS("/comp/test/1.0.0/bin/test"), fds, ARRAY_SIZE(fds), PRIO_DEFAULT, 0, NULL);
    if (IS_ERR(status))
    {
        printf("init: failed to create test process %Y\n", status);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
