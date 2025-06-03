#include <libpatchwork/patchwork.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/io.h>
#include <sys/proc.h>
#include <threads.h>

void spawn_program(const char* path, priority_t priority)
{
    if (path == NULL)
    {
        return;
    }

    fd_t klog = open("sys:/klog");
    const char* argv[] = {path, NULL};
    spawn_fd_t fds[] = {{.parent = klog, .child = STDOUT_FILENO}, SPAWN_FD_END};
    spawn_attr_t attr = {.priority = priority};
    spawn(argv, fds, "home:/usr", &attr);
    close(klog);
}

int main(void)
{
    config_t* config = config_open("init", "main");
    if (config == NULL)
    {
        printf("init: failed to open config file!\n");
        return EXIT_FAILURE;
    }

    priority_t servicePriority = config_get_int(config, "startup", "service_priority", 31);
    priority_t programPriority = config_get_int(config, "startup", "program_priority", 31);

    config_array_t* services = config_get_array(config, "startup", "services");
    if (services == NULL)
    {
        printf("init: failed to retrieve services from config file!\n");
        return EXIT_FAILURE;
    }

    for (uint64_t i = 0; i < config_array_length(services); i++)
    {
        const char* service = config_array_get_string(services, i, NULL);
        spawn_program(service, servicePriority);
    }

    config_array_t* serviceFiles = config_get_array(config, "startup", "service_files");
    for (uint64_t i = 0; i < config_array_length(services); i++)
    {
        const char* file = config_array_get_string(serviceFiles, i, "home:/");

        stat_t info;
        while (stat(file, &info) == ERR)
        {
            printf("test1\n");
            thrd_yield();
            printf("test2\n");
        }
    }

    config_array_t* programs = config_get_array(config, "startup", "programs");
    for (uint64_t i = 0; i < config_array_length(programs); i++)
    {
        const char* program = config_array_get_string(programs, i, NULL);
        spawn_program(program, programPriority);
    }

    config_close(config);
    return 0;
}
