#include <errno.h>
#include <libpatchwork/patchwork.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/io.h>
#include <sys/proc.h>
#include <threads.h>
#include <time.h>

static void init_spawn_pkgd(void)
{
    const char* argv[] = {"/sbin/pkgd", NULL};
    if (spawn(argv, SPAWN_DEFAULT) == ERR)
    {
        printf("init: failed to spawn pkgd! (%s)\n", strerror(errno));
        abort();
    }
}

static void init_create_launch_links(void)
{
    fd_t pkg = open("/pkg");
    if (pkg == ERR)
    {
        printf("init: failed to open /pkg! (%s)\n", strerror(errno));
        abort();
    }

    dirent_t* dirents;
    uint64_t amount;
    if (readdir(pkg, &dirents, &amount) == ERR)
    {
        close(pkg);
        printf("init: failed to read /pkg! (%s)\n", strerror(errno));
        abort();
    }
    close(pkg);

    for (uint64_t i = 0; i < amount; i++)
    {
        if (dirents[i].type != INODE_DIR || dirents[i].path[0] == '.')
        {
            continue;
        }

        printf("init: creating launch symlink for package '%s'\n", dirents[i].path);
        if (symlink("launch", F("/sys/bin/%s", dirents[i].path)) == ERR && errno != EEXIST)
        {
            free(dirents);
            printf("init: failed to create launch symlink for package '%s'! (%s)\n", dirents[i].path, strerror(errno));
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
        printf("init: failed to open config file! (%s)\n", strerror(errno));
        abort();
    }

    config_array_t* programs = config_get_array(config, "startup", "programs");
    for (uint64_t i = 0; i < programs->length; i++)
    {
        const char* argv[] = {programs->items[i], NULL};
        if (spawn(argv, SPAWN_DEFAULT) == ERR)
        {
            printf("init: failed to spawn program '%s' (%s)\n", programs->items[i], strerror(errno));
        }
        else
        {
            printf("init: spawned program '%s'\n", programs->items[i]);
        }
    }

    config_close(config);
}

int main(void)
{
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

    init_spawn_pkgd();

    init_create_launch_links();

    init_config_load();

    printf("init: all startup tasks completed!\n");

    return EXIT_SUCCESS;
}
