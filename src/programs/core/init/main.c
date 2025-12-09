#include <errno.h>
#include <libpatchwork/patchwork.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/io.h>
#include <sys/proc.h>
#include <threads.h>
#include <time.h>

/// @todo Replace the init process with a Lua script when Lua has been ported to PatchworkOS

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

static void spawn_program(const char* path, priority_t priority)
{
    if (path == NULL)
    {
        return;
    }

    const char* argv[] = {path, NULL};
    spawn_fd_t fds[] = {
        {.parent = STDOUT_FILENO, .child = STDOUT_FILENO},
        {.parent = STDERR_FILENO, .child = STDERR_FILENO},
        SPAWN_FD_END,
    };
    if (spawn(argv, fds, "/usr", priority, SPAWN_DEFAULT) == ERR)
    {
        printf("init: failed to spawn program '%s' (%s)\n", path, strerror(errno));
    }
}

static void start_services(config_t* config)
{
    priority_t servicePriority = config_get_int(config, "startup", "service_priority", 31);

    config_array_t* services = config_get_array(config, "startup", "services");
    for (uint64_t i = 0; i < services->length; i++)
    {
        spawn_program(services->items[i], servicePriority);
    }

    config_array_t* serviceFiles = config_get_array(config, "startup", "service_files");
    for (uint64_t i = 0; i < serviceFiles->length; i++)
    {
        clock_t start = uptime();
        stat_t info;
        while (stat(serviceFiles->items[i], &info) == ERR)
        {
            nanosleep(CLOCKS_PER_SEC / 100);
            if (uptime() - start > CLOCKS_PER_SEC)
            {
                printf("init: timeout waiting for service file '%s'\n", serviceFiles->items[i]);
                abort();
            }
        }
    }
}

static void start_programs(config_t* config)
{
    priority_t programPriority = config_get_int(config, "startup", "program_priority", 31);

    config_array_t* programs = config_get_array(config, "startup", "programs");
    for (uint64_t i = 0; i < programs->length; i++)
    {
        spawn_program(programs->items[i], programPriority);
    }
}

static void execute_commands(config_t* config)
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

    config_t* config = config_open("init", "main");
    if (config == NULL)
    {
        printf("init: failed to open config file! (%s)\n", strerror(errno));
        return EXIT_FAILURE;
    }

    printf("init: setting up environment...\n");
    environment_setup(config);

    printf("init: starting services...\n");
    start_services(config);
    printf("init: starting programs...\n");
    start_programs(config);

    printf("init: executing commands...\n");
    execute_commands(config);

    printf("init: all startup tasks completed!\n");
    config_close(config);
    return 0;
}
