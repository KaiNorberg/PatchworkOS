#include "root.h"

#include <errno.h>
#include <libpatchwork/patchwork.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/io.h>
#include <sys/proc.h>
#include <threads.h>
#include <time.h>

/**
 * @brief Init process.
 * @defgroup programs_init Init
 * @ingroup programs
 *
 * The init process is the first user-space program started by the kernel, due to this it can be thought of as the
 * "root" of all processes in the system.
 *
 * ## Filesystem Heirarchy
 *
 * Included below is a table indicating the permissions or visibility of directories as setup by the init process.
 *
 * <div align="center">
 * | Directory | Description | Permissions/Visibility |
 * |-----------|-------------|------------------------|
 * | /acpi     | ACPI Information | Read-Only |
 * | /bin      | System Binaries | Read and Execute |
 * | /cfg      | System Configuration Files | Read-Only |
 * | /dev      | Device Files | Read and Write |
 * | /efi      | UEFI Bootloader Files | Hidden |
 * | /home     | Available for any use | Exposed |
 * | /kernel   | Kernel Binaries | Hidden |
 * | /lib      | System Libraries | Read and Execute |
 * | /net      | Network Filesystem | Exposed |
 * | /proc     | Process Filesystem | Exposed |
 * | /sbin     | Essential System Binaries | Read and Execute |
 * | /usr      | User Binaries and Libraries | Read and Execute |
 * </div>
 */

static void environment_setup(config_t* config)
{
    uint64_t index = 0;
    while (true)
    {
        const char* value;
        const char* key;
        config_get(config, "environment", index++, NULL, &value, &key);
        if (value == NULL || key == NULL)
        {
            break;
        }

        fd_t env = open(F("/proc/self/env/%s:rwc", key));
        if (env == ERR)
        {
            printf("init: failed to open /proc/self/env/%s for writing (%s)\n", key, strerror(errno));
            continue;
        }

        if (swrite(env, value) == ERR)
        {
            printf("init: failed to write to /proc/self/env/%s (%s)\n", key, strerror(errno));
            close(env);
            continue;
        }

        close(env);
        printf("init: %s=%s\n", key, value);
    }
}

static void child_spawn(const char* path, priority_t priority)
{
    if (path == NULL)
    {
        return;
    }

    const char* argv[] = {path, NULL};
    pid_t pid = spawn(argv, SPAWN_SUSPEND | SPAWN_STDIO_FDS | SPAWN_EMPTY_GROUP | SPAWN_COPY_NS);
    if (pid == ERR)
    {
        printf("init: failed to spawn program '%s' (%s)\n", path, strerror(errno));
    }

    swritefile(F("/proc/%llu/prio", pid), F("%llu", priority));
    swritefile(F("/proc/%llu/cwd", pid), "/home");

    // Bind directories to themselves with new permissions and with the "L" (:locked) flag to ensure the child processes
    // cant unmount the directories and "S" (:sticky) to ensure children cant bypass the mount by mounting an ancestor.
    if (swritefile(F("/proc/%llu/ctl", pid),
            "bind /acpi /acpi:LSr && "
            "bind /bin /bin:LSrx && "
            "bind /cfg /cfg:LSr && "
            "bind /dev /dev:LSrw && "
            "bind /dev/null /efi:LS && "
            "bind /dev/null /kernel:LS && "
            "bind /lib /lib:LSrx && "
            "bind /sbin /sbin:LSrx && "
            "bind /usr /usr:LSrx && "
            "start") == ERR)
    {
        printf("init: failed to setup process namespaces for '%s' (%s)\n", path, strerror(errno));
    }
}

static void programs_start(config_t* config)
{
    priority_t programPriority = config_get_int(config, "startup", "program_priority", 31);

    config_array_t* programs = config_get_array(config, "startup", "programs");
    for (uint64_t i = 0; i < programs->length; i++)
    {
        child_spawn(programs->items[i], programPriority);
    }
}

static void commands_run(config_t* config)
{
    config_array_t* commands = config_get_array(config, "startup", "commands");
    for (uint64_t i = 0; i < commands->length; i++)
    {
        if (system(commands->items[i]) != 0)
        {
            printf("init: failed to execute command '%s' (%s)\n", commands->items[i], strerror(errno));
        }
    }
}

static void init_config_load(void)
{
    config_t* config = config_open("init", "main");
    if (config == NULL)
    {
        printf("init: failed to open config file! (%s)\n", strerror(errno));
        abort();
    }

    printf("init: setting up environment...\n");
    environment_setup(config);

    printf("init: starting programs...\n");
    programs_start(config);

    printf("init: executing commands...\n");
    commands_run(config);

    printf("init: all startup tasks completed!\n");
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

    init_config_load();

    root_start();

    return EXIT_SUCCESS;
}
