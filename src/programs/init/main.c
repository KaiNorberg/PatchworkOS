#include <libpatchwork/patchwork.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/io.h>
#include <sys/proc.h>
#include <threads.h>

static void spawn_program(const char* path, priority_t priority)
{
    if (path == NULL)
    {
        return;
    }

    fd_t klog = open("/dev/klog");
    if (klog == ERR)
    {
        printf("init: failed to open klog\n");
        return;
    }

    const char* argv[] = {path, NULL};
    spawn_fd_t fds[] = {
        {.parent = klog, .child = STDOUT_FILENO},
        {.parent = klog, .child = STDERR_FILENO},
        SPAWN_FD_END,
    };
    spawn_attr_t attr = {.priority = priority};
    if (spawn(argv, fds, "/usr", &attr) == ERR)
    {
        printf("init: failed to spawn program %s\n", path);
    }

    if (close(klog) == ERR)
    {
        printf("init: failed to close klog\n");
    }
}

static void start_services(config_t* config)
{
    priority_t servicePriority = config_get_int(config, "startup", "service_priority", 31);

    config_array_t* services = config_get_array(config, "startup", "services");
    for (uint64_t i = 0; i < config_array_length(services); i++)
    {
        const char* service = config_array_get_string(services, i, NULL);
        spawn_program(service, servicePriority);
    }

    config_array_t* serviceFiles = config_get_array(config, "startup", "service_files");
    for (uint64_t i = 0; i < config_array_length(serviceFiles); i++)
    {
        const char* file = config_array_get_string(serviceFiles, i, "/");

        stat_t info;
        while (stat(file, &info) == ERR)
        {
            thrd_yield();
        }
    }
}

static void start_programs(config_t* config)
{
    priority_t programPriority = config_get_int(config, "startup", "program_priority", 31);

    config_array_t* programs = config_get_array(config, "startup", "programs");
    for (uint64_t i = 0; i < config_array_length(programs); i++)
    {
        const char* program = config_array_get_string(programs, i, NULL);
        spawn_program(program, programPriority);
    }
}

static void execute_commands(config_t* config)
{
    config_array_t* commands = config_get_array(config, "startup", "commands");
    for (uint64_t i = 0; i < config_array_length(commands); i++)
    {
        const char* command = config_array_get_string(commands, i, NULL);
        if (system(command) != 0)
        {
            printf("init: failed to execute command '%s'\n", command);
        }
    }
}

int main(void)
{
    config_t* config = config_open("init", "main");
    if (config == NULL)
    {
        printf("init: failed to open config file!\n");
        return EXIT_FAILURE;
    }

    start_services(config);
    start_programs(config);

    execute_commands(config);

    config_close(config);
    return 0;
}
