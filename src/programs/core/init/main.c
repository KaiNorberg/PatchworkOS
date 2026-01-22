#include <errno.h>
#include <libpatchwork/patchwork.h>
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

static uint64_t init_socket_addr_wait(const char* family, const char* addr)
{
    fd_t addrs = open(F("/net/%s/addrs", family));
    if (addrs == ERR)
    {
        return ERR;
    }

    clock_t start = uptime();
    while (true)
    {
        nanosleep(CLOCKS_PER_SEC / 10);

        const char* data = readfiles(F("/net/%s/addrs", family));
        if (data == NULL)
        {
            close(addrs);
            return ERR;
        }

        if (strstr(data, addr) != NULL)
        {
            free((void*)data);
            break;
        }

        free((void*)data);

        if ((uptime() - start) >= CLOCKS_PER_SEC * 10)
        {
            close(addrs);
            return ERR;
        }
    }

    close(addrs);
    return 0;
}

static void init_root_ns(void)
{
    if (mount("/dev:rwL", "/sys/fs/devfs", NULL) == ERR)
    {
        printf("init: failed to mount devfs (%s)\n", strerror(errno));
        abort();
    }

    if (mount("/net:rwL", "/sys/fs/netfs", NULL) == ERR)
    {
        printf("init: failed to mount netfs (%s)\n", strerror(errno));
        abort();
    }

    if (mount("/proc:rwL", "/sys/fs/procfs", NULL) == ERR)
    {
        printf("init: failed to mount procfs (%s)\n", strerror(errno));
        abort();
    }

    if (mount("/tmp:rwL", "/sys/fs/tmpfs", NULL) == ERR)
    {
        printf("init: failed to mount tmpfs (%s)\n", strerror(errno));
        abort();
    }
}

static void init_spawn_boxd(void)
{
    const char* argv[] = {"/sbin/boxd", NULL};
    if (spawn(argv, SPAWN_DEFAULT) == ERR)
    {
        printf("init: failed to spawn boxd (%s)\n", strerror(errno));
        abort();
    }

    if (init_socket_addr_wait("local", "boxspawn") == ERR)
    {
        printf("init: timeout waiting for boxd to create boxspawn socket (%s)\n", strerror(errno));
        abort();
    }
}

static void init_create_pkg_links(void)
{
    fd_t box = open("/box");
    if (box == ERR)
    {
        printf("init: failed to open /box (%s)\n", strerror(errno));
        abort();
    }

    dirent_t* dirents;
    uint64_t amount;
    if (readdir(box, &dirents, &amount) == ERR)
    {
        close(box);
        printf("init: failed to read /box (%s)\n", strerror(errno));
        abort();
    }
    close(box);

    for (uint64_t i = 0; i < amount; i++)
    {
        if (dirents[i].type != INODE_DIR || dirents[i].path[0] == '.')
        {
            continue;
        }

        if (symlink("boxspawn", F("/base/bin/%s", dirents[i].path)) == ERR && errno != EEXIST)
        {
            free(dirents);
            printf("init: failed to create launch symlink for box '%s' (%s)\n", dirents[i].path, strerror(errno));
            abort();
        }
    }

    free(dirents);
}

static void init_config_load(void)
{
    config_t* config = config_open("init", "main");
    if (config == NULL)
    {
        printf("init: failed to open config file (%s)\n", strerror(errno));
        abort();
    }

    config_array_t* services = config_get_array(config, "startup", "services");
    for (uint64_t i = 0; i < services->length; i++)
    {
        nanosleep(CLOCKS_PER_MS);
        printf("init: spawned service '%s'\n", services->items[i]);
        const char* argv[] = {services->items[i], NULL};
        if (spawn(argv, SPAWN_EMPTY_FDS | SPAWN_EMPTY_ENV | SPAWN_EMPTY_CWD | SPAWN_EMPTY_GROUP) == ERR)
        {
            printf("init: failed to spawn service '%s' (%s)\n", services->items[i], strerror(errno));
        }
    }

    config_array_t* sockets = config_get_array(config, "startup", "sockets");
    for (uint64_t i = 0; i < sockets->length; i++)
    {
        if (init_socket_addr_wait("local", sockets->items[i]) == ERR)
        {
            printf("init: timeout waiting for socket '%s' (%s)\n", sockets->items[i], strerror(errno));
        }
    }

    config_array_t* programs = config_get_array(config, "startup", "programs");
    for (uint64_t i = 0; i < programs->length; i++)
    {
        nanosleep(CLOCKS_PER_MS);
        printf("init: spawn program '%s'\n", programs->items[i]);
        const char* argv[] = {programs->items[i], NULL};
        if (spawn(argv, SPAWN_EMPTY_FDS | SPAWN_EMPTY_ENV | SPAWN_EMPTY_CWD | SPAWN_EMPTY_GROUP) == ERR)
        {
            printf("init: failed to spawn program '%s' (%s)\n", programs->items[i], strerror(errno));
        }
    }

    config_close(config);
}

int main(void)
{
    init_root_ns();

    fd_t klog = open("/dev/klog:rw");
    if (klog == ERR)
    {
        return EXIT_FAILURE;
    }
    if (dup2(klog, STDOUT_FILENO) == ERR || dup2(klog, STDERR_FILENO) == ERR)
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
