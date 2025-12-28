#include "manifest.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/argsplit.h>
#include <sys/io.h>
#include <sys/proc.h>

/**
 * @brief Package Daemon.
 * @defgroup programs_pkgd Package Daemon
 * @ingroup programs
 *
 */

#define ARGV_MAX 512
#define BUFFER_MAX 0x1000

static void pkg_kill(pid_t pid)
{
    swritefile(F("/sys/proc/%llu/ctl", pid), "kill");
}

static uint64_t pkg_spawn(const char* buffer)
{
    char argBuffer[BUFFER_MAX];
    uint64_t argc;
    const char** argv = argsplit_buf(argBuffer, sizeof(argBuffer), buffer, BUFFER_MAX, &argc);
    if (argv == NULL || argc == 0)
    {
        printf("pkgd: failed to parse arguments\n");
        return ERR;
    }

    if (strchr(argv[0], '/') != NULL || strchr(argv[0], '.') != NULL)
    {
        printf("pkgd: invalid package name '%s'\n", argv[0]);
        return ERR;
    }

    printf("pkgd: spawning package '%s'\n", argv[0]);

    manifest_t manifest;
    if (manifest_parse(F("/pkg/%s/manifest", argv[0]), &manifest) == ERR)
    {
        printf("pkgd: failed to parse manifest of '%s'\n", argv[0]);
        return ERR;
    }

    substitution_t substitutions[] = {
        {"PKG", F("/pkg/%s/", argv[0])},
    };
    manifest_substitute(&manifest, substitutions, sizeof(substitutions) / sizeof(substitution_t));

    section_t* exec = &manifest.sections[SECTION_EXEC];
    char* bin = manifest_get_value(exec, "bin");
    if (bin == NULL)
    {
        printf("pkgd: manifest of '%s' missing 'bin' entry\n", argv[0]);
        return ERR;
    }

    if (bin[0] == '/')
    {
        printf("pkgd: manifest of '%s' has invalid 'bin' entry (must be relative path)\n", argv[0]);
        return ERR;
    }

    uint64_t priority = manifest_get_integer(exec, "priority");
    if (priority == ERR)
    {
        printf("pkgd: manifest of '%s' invalid or missing 'priority' entry\n", argv[0]);
        return ERR;
    }

    const char* temp = argv[0];
    argv[0] = F("/pkg/%s/%s", argv[0], bin);
    pid_t pid = spawn(argv, SPAWN_SUSPEND | SPAWN_EMPTY_FDS | SPAWN_EMPTY_ENV | SPAWN_EMPTY_GROUP | SPAWN_EMPTY_NS);
    if (pid == ERR)
    {
        printf("pkgd: failed to spawn '%s' (%s)\n", bin, strerror(errno));
        return ERR;
    }
    argv[0] = temp;

    if (swritefile(F("/proc/%llu/prio", pid), F("%llu", priority)) == ERR)
    {
        pkg_kill(pid);
        printf("pkgd: failed to set priority of '%s' (%s)\n", argv[0], strerror(errno));
        return ERR;
    }

    section_t* env = &manifest.sections[SECTION_ENV];
    for (uint64_t i = 0; i < env->amount; i++)
    {
        if (swritefile(F("/proc/%llu/env/%s:cw", pid, env->entries[i].key), env->entries[i].value) == ERR)
        {
            pkg_kill(pid);
            printf("pkgd: failed to set environment variable '%s' (%s)\n", env->entries[i].key, strerror(errno));
            return ERR;
        }
    }

    if (swritefile(F("/proc/%llu/ctl", pid), F("mount /:LSrwx tmpfs", argv[0])) == ERR)
    {
        pkg_kill(pid);
        printf("pkgd: failed to set root of '%s' (%s)\n", argv[0], strerror(errno));
        return ERR;
    }

    section_t* namespace = &manifest.sections[SECTION_NAMESPACE];
    for (uint64_t i = 0; i < namespace->amount; i++)
    {
        char* key = namespace->entries[i].key;
        char* value = namespace->entries[i].value;

        if (swritefile(F("/proc/%llu/ctl", pid), F("touch %s:rwcp && bind %s %s", key, key, value)) == ERR)
        {
            pkg_kill(pid);
            printf("pkgd: failed to bind '%s' to '%s' (%s)\n", key, value, strerror(errno));
            return ERR;
        }
    }

    return 0;
}

int main(void)
{
    char* id = sreadfile("/net/local/seqpacket");
    if (id == NULL)
    {
        printf("pkgd: failed to open local seqpacket socket (%s)\n", strerror(errno));
        abort();
    }

    if (swritefile(F("/net/local/%s/ctl", id), "bind pkg && listen") == ERR)
    {
        printf("pkgd: failed to bind to pkg (%s)\n", strerror(errno));
        goto error;
    }

    printf("pkgd: listening for connections...\n");
    while (1)
    {
        fd_t client = open(F("/net/local/%s/accept", id));
        if (client == ERR)
        {
            printf("pkgd: failed to accept connection (%s)\n", strerror(errno));
            goto error;
        }

        char buffer[BUFFER_MAX] = {0};
        if (read(client, buffer, sizeof(buffer) - 1) == ERR)
        {
            printf("pkgd: failed to read pkg (%s)\n", strerror(errno));
            close(client);
            continue;
        }

        if (pkg_spawn(buffer) == ERR)
        {
            close(client);
            continue;
        }

        close(client);
    }

error:
    free(id);
    return EXIT_FAILURE;
}