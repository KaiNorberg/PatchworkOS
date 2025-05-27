#include <libpatchwork/patchwork.h>
#include <sys/argsplit.h>
#include <sys/io.h>
#include <sys/proc.h>
#include <threads.h>

void spawn_program(const char* path)
{
    if (path == NULL)
    {
        return;
    }

    fd_t klog = open("sys:/klog");
    const char* argv[] = {path, NULL};
    spawn_fd_t fds[] = {{.parent = klog, .child = STDOUT_FILENO}, SPAWN_FD_END};
    spawn(argv, fds);
    close(klog);
}

#include <stdio.h>

int main(void)
{
    chdir("home:/usr");

    config_t* config = config_open("init", "main");

    config_array_t* services = config_array_get(config, "startup", "services");
    for (uint64_t i = 0; i < config_array_length(services); i++)
    {
        const char* service = config_array_string_get(services, i, NULL);
        spawn_program(service);
    }

    config_array_t* serviceFiles = config_array_get(config, "startup", "service_files");
    for (uint64_t i = 0; i < config_array_length(services); i++)
    {
        const char* file = config_array_string_get(serviceFiles, i, "home:/");

        stat_t info;
        while (stat(file, &info) == ERR)
        {
            thrd_yield();
        }
    }

    config_array_t* programs = config_array_get(config, "startup", "programs");
    for (uint64_t i = 0; i < config_array_length(programs); i++)
    {
        const char* program = config_array_string_get(programs, i, NULL);
        spawn_program(program);
    }

    config_close(config);
    return 0;
}
