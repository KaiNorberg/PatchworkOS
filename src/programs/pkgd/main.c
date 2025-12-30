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
 * The package daemon is responsible for spawning and managing packages.
 *
 * ## Spawning Packages
 *
 * To spawn a package a request should be sent to the "pkg-spawn" socket, included below is the format of the request.
 *
 * ```
 * [key=value ...] -- <package_name> [arg1 arg2 ...]
 * ```
 *
 * Where the following key values can be specified:
 * - `stdin`: The key for the shared file descriptor to use as standard input.
 * - `stdout`: The key for the shared file descriptor to use as standard output.
 * - `stderr`: The key for the shared file descriptor to use as standard error.
 *
 * @note Certain key values will only be used if the package is a foreground package.
 *
 * @todo Once filesystem servers are implemented the package deamon should use them instead of sockets.
 *
 * @{
 */

#define ARGV_MAX 512
#define BUFFER_MAX 0x1000

static void pkg_kill(pid_t pid)
{
    swritefile(F("/sys/proc/%llu/ctl", pid), "kill");
}

static uint64_t pkg_spawn(const char* buffer)
{
    printf("pkgd: received spawn request: '%s'\n", buffer);

    char argBuffer[BUFFER_MAX];
    uint64_t argc;
    const char** argv = argsplit_buf(argBuffer, sizeof(argBuffer), buffer, BUFFER_MAX, &argc);
    if (argv == NULL || argc == 0)
    {
        printf("pkgd: failed to parse arguments\n");
        return ERR;
    }

    const char* pkg = NULL;
    for (uint64_t i = 0; i < argc; i++)
    {
        if (strcmp(argv[i], "--") == 0)
        {
            if (i + 1 >= argc)
            {
                printf("pkgd: missing package name\n");
                return ERR;
            }

            pkg = argv[i + 1];
            break;
        }

        /// @todo Implement handling of key value pairs.
    }

    if (pkg == NULL)
    {
        printf("pkgd: missing package name\n");
        return ERR;
    }

    if (strchr(pkg, '/') != NULL || strchr(pkg, '.') != NULL)
    {
        printf("pkgd: invalid package name '%s'\n", pkg);
        return ERR;
    }

    manifest_t manifest;
    if (manifest_parse(F("/pkg/%s/manifest", pkg), &manifest) == ERR)
    {
        printf("pkgd: failed to parse manifest of '%s'\n", pkg);
        return ERR;
    }

    substitution_t substitutions[] = {
        {"PKG", F("/pkg/%s/", pkg)},
    };
    manifest_substitute(&manifest, substitutions, sizeof(substitutions) / sizeof(substitution_t));

    section_t* exec = &manifest.sections[SECTION_EXEC];
    char* bin = manifest_get_value(exec, "bin");
    if (bin == NULL)
    {
        printf("pkgd: manifest of '%s' missing 'bin' entry\n", pkg);
        return ERR;
    }

    uint64_t priority = manifest_get_integer(exec, "priority");
    if (priority == ERR)
    {
        printf("pkgd: manifest of '%s' invalid or missing 'priority' entry\n", pkg);
        return ERR;
    }

    spawn_flags_t flags = (SPAWN_SUSPEND | SPAWN_EMPTY_ALL) & ~SPAWN_EMPTY_NS;

    section_t* sandbox = &manifest.sections[SECTION_SANDBOX];
    char* profile = manifest_get_value(sandbox, "profile");
    if (profile == NULL)
    {
        profile = "empty";
    }

    if (strcmp(profile, "empty") == 0)
    {
        flags |= SPAWN_EMPTY_NS;
    }
    else if (strcmp(profile, "copy") == 0)
    {
        flags |= SPAWN_COPY_NS;
    }
    else if (strcmp(profile, "share") == 0)
    {
        // Do nothing, default is to share namespace.
    }
    else
    {
        printf("pkgd: manifest of '%s' has unknown sandbox profile '%s'\n", argv[0], profile);
        return ERR;
    }

    const char* temp = argv[0];
    argv[0] = bin;
    pid_t pid = spawn(argv, flags);
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

    fd_t ctl = open(F("/proc/%llu/ctl", pid));
    if (ctl == ERR)
    {
        pkg_kill(pid);
        printf("pkgd: failed to open ctl of '%s' (%s)\n", pkg, strerror(errno));
        return ERR;
    }

    if (swrite(ctl, "mount /:LSrwx tmpfs") == ERR)
    {
        close(ctl);
        pkg_kill(pid);
        printf("pkgd: failed to set root of '%s' (%s)\n", pkg, strerror(errno));
        return ERR;
    }

    section_t* namespace = &manifest.sections[SECTION_NAMESPACE];
    for (uint64_t i = 0; i < namespace->amount; i++)
    {
        char* key = namespace->entries[i].key;
        char* value = namespace->entries[i].value;

        if (swrite(ctl, F("touch %s:rwcp && bind %s %s", key, key, value)) == ERR)
        {
            close(ctl);
            pkg_kill(pid);
            printf("pkgd: failed to bind '%s' to '%s' (%s)\n", key, value, strerror(errno));
            return ERR;
        }
    }

    if (swrite(ctl, "start") == ERR)
    {
        close(ctl);
        pkg_kill(pid);
        printf("pkgd: failed to start '%s' (%s)\n", pkg, strerror(errno));
        return ERR;
    }

    close(ctl);

    printf("pkgd: spawned package '%s' with pid %llu\n", pkg, pid);
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

    if (swritefile(F("/net/local/%s/ctl", id), "bind pkg-spawn && listen") == ERR)
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