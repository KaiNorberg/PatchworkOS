#include <errno.h>
#include <patchwork/patchwork.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fs.h>
#include <sys/proc.h>
#include <threads.h>
#include <time.h>

/**
 * @brief Init Process.
 * @defgroup programs_init Init
 * @ingroup programs
 *
 * The init process is the first user space process started by the kernel. It is responsible for setting up the "root
 * namespace", the namespace the init process and boxd run in, and for spawning initial processes.
 *
 * ## Root Namespace
 *
 * The init process creates the root namespace, which is the parent of all other user-space namespaces. Included below
 * is an overview of the root namespace.
 *
 * <div align="center">
 * | Name                                | Type      | Description                                |
 * |-------------------------------------|-----------|--------------------------------------------|
 * | `/base`                             | directory | Base system directory.                     |
 * | `/base/bin`                         | directory | Non-essential system binaries.             |
 * | `/base/lib`                         | directory | System libraries.                          |
 * | `/base/include`                     | directory | System header files.                       |
 * | `/base/data`                        | directory | System data files.                         |
 * | `/box`                              | directory | Installed boxes directory.                 |
 * | `/cfg`                              | directory | System configuration files.                |
 * | `/dev`                              | devfs     | Device filesystem.                         |
 * | `/efi`                              | directory | EFI files.                                 |
 * | `/efi/boot`                         | directory | EFI bootloader files.                      |
 * | `/kernel`                           | directory | Kernel related files.                      |
 * | `/kernel/modules`                   | directory | Kernel modules directory.                  |
 * | `/kernel/modules/<kernel_verion>`   | directory | Version specific kernel modules.           |
 * | `/net`                              | netfs     | Network filesystem.                        |
 * | `/proc`                             | procfs    | Process filesystem.                        |
 * | `/sbin`                             | directory | Essential system binaries.                 |
 * | `/sys`                              | sysfs     | System filesystem, mounted by the kernel.  |
 * | `/tmp`                              | tmpfs     | Temporary filesystem.                      |
 * </div>
 *
 */

static status_t init_socket_addr_wait(const char* family, const char* addr)
{
    fd_t addrs;
    status_t status = open(&addrs, F("/net/%s/addrs", family));
    if (IS_ERR(status))
    {
        return status;
    }

    clock_t start = uptime();
    while (true)
    {
        nanosleep(CLOCKS_PER_SEC / 10);

        char* data;
        status = readfiles(&data, F("/net/%s/addrs", family));
        if (IS_ERR(status))
        {
            close(addrs);
            return status;
        }

        if (strstr(data, addr) != NULL)
        {
            free(data);
            break;
        }

        free(data);

        if ((uptime() - start) >= CLOCKS_PER_SEC * 10)
        {
            close(addrs);
            return ERR(USER, TIMEOUT);
        }
    }

    close(addrs);
    return OK;
}

static void init_root_ns(void)
{
    status_t status = mount("/dev:rwL", "/sys/fs/devfs", NULL);
    if (IS_ERR(status))
    {
        printf("init: failed to mount devfs %Y\n", status);
        abort();
    }

    status = mount("/net:rwL", "/sys/fs/netfs", NULL);
    if (IS_ERR(status))
    {
        printf("init: failed to mount netfs %Y\n", status);
        abort();
    }

    status = mount("/proc:rwL", "/sys/fs/procfs", NULL);
    if (IS_ERR(status))
    {
        printf("init: failed to mount procfs %Y\n", status);
        abort();
    }

    status = mount("/tmp:rwL", "/sys/fs/tmpfs", NULL);
    if (IS_ERR(status))
    {
        printf("init: failed to mount tmpfs %Y\n", status);
        abort();
    }
}

static void init_spawn_boxd(void)
{
    const char* argv[] = {"/sbin/boxd", NULL};
    status_t status = spawn(argv, SPAWN_DEFAULT, NULL);
    if (IS_ERR(status))
    {
        printf("init: failed to spawn boxd %Y\n", status);
        abort();
    }

    status = init_socket_addr_wait("local", "boxspawn");
    if (IS_ERR(status))
    {
        printf("init: timeout waiting for boxd to create boxspawn socket %Y\n", status);
        abort();
    }
}

static void init_create_pkg_links(void)
{
    status_t status;

    fd_t box;
    status = open(&box, "/box");
    if (IS_ERR(status))
    {
        printf("init: failed to open /box %Y\n", status);
        abort();
    }

    dirent_t* dirents;
    uint64_t amount;
    status = readdir(box, &dirents, &amount);
    if (IS_ERR(status))
    {
        close(box);
        printf("init: failed to read /box %Y\n", status);
        abort();
    }
    close(box);

    for (uint64_t i = 0; i < amount; i++)
    {
        if (dirents[i].type != VDIR || dirents[i].path[0] == '.')
        {
            continue;
        }

        status = symlink("boxspawn", F("/base/bin/%s", dirents[i].path));
        if (IS_ERR(status) && !IS_CODE(status, EXIST))
        {
            free(dirents);
            printf("init: failed to create launch symlink for box '%s' %Y\n", dirents[i].path, status);
            abort();
        }
    }

    free(dirents);
}

static void init_config_load(void)
{
    status_t status;

    config_t* config = config_open("init", "main");
    if (config == NULL)
    {
        printf("init: failed to open config file\n");
        abort();
    }

    config_array_t* services = config_get_array(config, "startup", "services");
    for (uint64_t i = 0; i < services->length; i++)
    {
        nanosleep(CLOCKS_PER_MS);
        printf("init: spawned service '%s'\n", services->items[i]);
        const char* argv[] = {services->items[i], NULL};
        status = spawn(argv, SPAWN_EMPTY_FDS | SPAWN_EMPTY_ENV | SPAWN_EMPTY_CWD | SPAWN_EMPTY_GROUP, NULL);
        if (IS_ERR(status))
        {
            printf("init: failed to spawn service '%s' %Y\n", services->items[i], status);
        }
    }

    config_array_t* sockets = config_get_array(config, "startup", "sockets");
    for (uint64_t i = 0; i < sockets->length; i++)
    {
        status = init_socket_addr_wait("local", sockets->items[i]);
        if (IS_ERR(status))
        {
            printf("init: timeout waiting for socket '%s' %Y\n", sockets->items[i], status);
        }
    }

    config_array_t* programs = config_get_array(config, "startup", "programs");
    for (uint64_t i = 0; i < programs->length; i++)
    {
        nanosleep(CLOCKS_PER_MS);
        printf("init: spawn program '%s'\n", programs->items[i]);
        const char* argv[] = {programs->items[i], NULL};
        status = spawn(argv, SPAWN_EMPTY_FDS | SPAWN_EMPTY_ENV | SPAWN_EMPTY_CWD | SPAWN_EMPTY_GROUP, NULL);
        if (IS_ERR(status))
        {
            printf("init: failed to spawn program '%s' %Y\n", programs->items[i], status);
        }
    }

    config_close(config);
}

int main(void)
{
    init_root_ns();

    fd_t klog;
    if (IS_ERR(open(&klog, "/dev/klog:rw")))
    {
        return EXIT_FAILURE;
    }
    fd_t stdoutFd = STDOUT_FILENO;
    fd_t stderrFd = STDERR_FILENO;
    if (IS_ERR(dup(klog, &stdoutFd)) || IS_ERR(dup(klog, &stderrFd)))
    {
        close(klog);
        return EXIT_FAILURE;
    }
    close(klog);

    init_spawn_boxd();

    init_create_pkg_links();

    init_config_load();

    printf("init: all startup tasks completed!\n");

    return EXIT_SUCCESS;
}
