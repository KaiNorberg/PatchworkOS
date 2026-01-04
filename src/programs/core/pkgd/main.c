#include "manifest.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/argsplit.h>
#include <sys/defs.h>
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
 * To spawn a package a request should be sent to the "pkgspawn" socket in the format:
 *
 * ```
 * [key=value ...] -- <package_name> [arg1 arg2 ...]
 * ```
 *
 * Where the following values can be specified:
 * - `stdin`: A shared file descriptor to use as standard input.
 * - `stdout`: A shared file descriptor to use as standard output.
 * - `stderr`: A shared file descriptor to use as standard error.
 * - `group`: A shared file descriptor to use as the process group (`/proc/[pid]/group`)
 * - `namespace`: A shared file descriptor to use as the process namespace (`/proc/[pid]/ns`).
 *
 * @note The `stdin`, `stdout`, `stderr` and `group` values will only be used if the package is a foreground package,
 * meanwhile the `namespace` will only be used if the package uses the `inherit` sandbox profile.
 *
 * @todo Implement group and namespace specification for foreground packages and the inherit profile.
 *
 * The "pkgspawn" socket will send a response in the format:
 *
 * ```
 * <background|foreground [key]|error [msg]>
 * ```
 *
 * On success, the response will either contain `background` if the package is a background package, or `foreground`
 * followed by a key for the packages `/proc/[pid]/wait` file if the package is a foreground package.
 *
 * On failure, the response will contain `error` followed by an error message.
 *
 * @todo Once filesystem servers are implemented the package deamon should use them instead of sockets.
 *
 * @{
 */

#define ARGV_MAX 512
#define BUFFER_MAX 0x1000

typedef struct
{
    char input[BUFFER_MAX];
    char result[BUFFER_MAX];
} pkg_spawn_t;

typedef struct
{
    const char* pkg;
    fd_t stdio[3];
    fd_t group;
    fd_t namespace;
} pkg_args_t;

static uint64_t pkg_args_parse(pkg_args_t* args, uint64_t argc, const char** argv, pkg_spawn_t* ctx)
{
    for (uint64_t i = 0; i < argc; i++)
    {
        if (strcmp(argv[i], "--") == 0)
        {
            if (i + 1 >= argc)
            {
                snprintf(ctx->result, sizeof(ctx->result), "error due to missing package name");
                return ERR;
            }

            args->pkg = argv[i + 1];
            break;
        }

        char* equalSign = strchr(argv[i], '=');
        if (equalSign == NULL)
        {
            continue;
        }

        *equalSign = '\0';

        const char* key = argv[i];
        const char* value = equalSign + 1;

        if (strcmp(key, "stdin") == 0)
        {
            args->stdio[STDIN_FILENO] = claim(value);
            if (args->stdio[STDIN_FILENO] == ERR)
            {
                snprintf(ctx->result, sizeof(ctx->result), "error due to invalid stdin");
                return ERR;
            }
        }
        else if (strcmp(key, "stdout") == 0)
        {
            args->stdio[STDOUT_FILENO] = claim(value);
            if (args->stdio[STDOUT_FILENO] == ERR)
            {
                snprintf(ctx->result, sizeof(ctx->result), "error due to invalid stdout");
                return ERR;
            }
        }
        else if (strcmp(key, "stderr") == 0)
        {
            args->stdio[STDERR_FILENO] = claim(value);
            if (args->stdio[STDERR_FILENO] == ERR)
            {
                snprintf(ctx->result, sizeof(ctx->result), "error due to invalid stderr");
                return ERR;
            }
        }
        else if (strcmp(key, "group") == 0)
        {
            args->group = claim(value);
            if (args->group == ERR)
            {
                snprintf(ctx->result, sizeof(ctx->result), "error due to invalid group");
                return ERR;
            }
        }
        else if (strcmp(key, "namespace") == 0)
        {
            args->namespace = claim(value);
            if (args->namespace == ERR)
            {
                snprintf(ctx->result, sizeof(ctx->result), "error due to invalid namespace");
                return ERR;
            }
        }
        else
        {
            snprintf(ctx->result, sizeof(ctx->result), "error due to unknown argument '%s'", key);
            return ERR;
        }
    }

    if (args->pkg == NULL || strchr(args->pkg, '/') != NULL || strchr(args->pkg, '.') != NULL)
    {
        snprintf(ctx->result, sizeof(ctx->result), "error due to missing package name");
        return ERR;
    }

    return 0;
}

static void pkg_spawn(pkg_spawn_t* ctx)
{
    pkg_args_t args = {.pkg = NULL, .stdio = {FD_NONE}, .group = FD_NONE, .namespace = FD_NONE};
    fd_t ctl = FD_NONE;
    pid_t pid = ERR;

    char argBuffer[BUFFER_MAX];
    uint64_t argc;
    const char** argv = argsplit_buf(argBuffer, sizeof(argBuffer), ctx->input, BUFFER_MAX, &argc);
    if (argv == NULL || argc == 0)
    {
        snprintf(ctx->result, sizeof(ctx->result), "error due to invalid request");
        goto error;
    }

    if (pkg_args_parse(&args, argc, argv, ctx) == ERR)
    {
        goto error;
    }

    manifest_t manifest;
    if (manifest_parse(F("/pkg/%s/manifest", args.pkg), &manifest) == ERR)
    {
        snprintf(ctx->result, sizeof(ctx->result), "error due to invalid manifest for package '%s'", args.pkg);
        goto error;
    }

    substitution_t substitutions[] = {
        {"PKG", F("/pkg/%s/", args.pkg)},
    };
    manifest_substitute(&manifest, substitutions, ARRAY_SIZE(substitutions));

    section_t* exec = &manifest.sections[SECTION_EXEC];
    char* bin = manifest_get_value(exec, "bin");
    if (bin == NULL)
    {
        snprintf(ctx->result, sizeof(ctx->result), "error due to manifest of '%s' missing 'bin' entry", args.pkg);
        goto error;
    }

    uint64_t priority = manifest_get_integer(exec, "priority");
    if (priority == ERR)
    {
        snprintf(ctx->result, sizeof(ctx->result), "error due to manifest of '%s' missing 'priority' entry", args.pkg);
        goto error;
    }

    section_t* sandbox = &manifest.sections[SECTION_SANDBOX];
    const char* profile = manifest_get_value(sandbox, "profile");
    if (profile == NULL)
    {
        profile = "empty";
    }

    const char* foreground = manifest_get_value(sandbox, "foreground");
    bool isForeground = foreground != NULL && strcmp(foreground, "true") == 0;
    bool shouldInheritNamespace = false;

    spawn_flags_t flags = SPAWN_SUSPEND | SPAWN_EMPTY_ENV | SPAWN_EMPTY_CWD | SPAWN_EMPTY_GROUP;
    if (strcmp(profile, "empty") == 0)
    {
        flags |= SPAWN_EMPTY_NS;
    }
    else if (strcmp(profile, "inherit") == 0)
    {
        shouldInheritNamespace = true;
    }
    else
    {
        snprintf(ctx->result, sizeof(ctx->result), "error due to manifest of '%s' having invalid 'profile' entry",
            args.pkg);
        goto error;
    }

    const char* temp = argv[0];
    argv[0] = bin;
    pid = spawn(argv, flags);
    if (pid == ERR)
    {
        snprintf(ctx->result, sizeof(ctx->result), "error due to spawn failure for '%s' (%s)", args.pkg,
            strerror(errno));
        goto error;
    }
    argv[0] = temp;

    if (swritefile(F("/proc/%llu/prio", pid), F("%llu", priority)) == ERR)
    {
        snprintf(ctx->result, sizeof(ctx->result), "error due to priority failure for '%s' (%s)", args.pkg,
            strerror(errno));
        goto error;
    }

    section_t* env = &manifest.sections[SECTION_ENV];
    for (uint64_t i = 0; i < env->amount; i++)
    {
        if (swritefile(F("/proc/%llu/env/%s:cw", pid, env->entries[i].key), env->entries[i].value) == ERR)
        {
            snprintf(ctx->result, sizeof(ctx->result), "error due to env var failure for '%s' (%s)", args.pkg,
                strerror(errno));
            goto error;
        }
    }

    ctl = open(F("/proc/%llu/ctl", pid));
    if (ctl == ERR)
    {
        snprintf(ctx->result, sizeof(ctx->result), "error due to ctl open failure for '%s' (%s)", args.pkg,
            strerror(errno));
        goto error;
    }

    if (shouldInheritNamespace)
    {
        if (swrite(ctl, F("setns %llu", args.namespace)) == ERR)
        {
            snprintf(ctx->result, sizeof(ctx->result), "error due to setns failure for '%s' (%s)", args.pkg,
                strerror(errno));
            goto error;
        }
    }
    else
    {
        if (swrite(ctl, "mount /:Lrwx tmpfs") == ERR)
        {
            snprintf(ctx->result, sizeof(ctx->result), "error due to root mount failure for '%s' (%s)", args.pkg,
                strerror(errno));
            goto error;
        }
    }

    section_t* namespace = &manifest.sections[SECTION_NAMESPACE];
    for (uint64_t i = 0; i < namespace->amount; i++)
    {
        char* key = namespace->entries[i].key;
        char* value = namespace->entries[i].value;

        if (swrite(ctl, F("touch %s:rwcp && bind %s %s", key, key, value)) == ERR)
        {
            printf("pkgd: failed to bind '%s' to '%s' (%s)\n", key, value, strerror(errno));
            goto error;
        }
    }

    if (isForeground)
    {
        for (uint8_t i = 0; i < 3; i++)
        {
            if (args.stdio[i] == FD_NONE)
            {
                continue;
            }

            if (swrite(ctl, F("dup2 %llu %llu", args.stdio[i], i)) == ERR)
            {
                snprintf(ctx->result, sizeof(ctx->result), "error due to dup2 failure for '%s' (%s)", args.pkg,
                    strerror(errno));
                goto error;
            }
        }

        if (swrite(ctl, F("setgroup %llu", args.group)) == ERR)
        {
            snprintf(ctx->result, sizeof(ctx->result), "error due to setns failure for '%s' (%s)", args.pkg,
                strerror(errno));
            goto error;
        }

        if (swrite(ctl, "close 3 -1") == ERR)
        {
            snprintf(ctx->result, sizeof(ctx->result), "error due to close failure for '%s' (%s)", args.pkg,
                strerror(errno));
            goto error;
        }

        fd_t wait = open(F("/proc/%llu/wait", pid));
        if (wait == ERR)
        {
            snprintf(ctx->result, sizeof(ctx->result), "error due to wait open failure for '%s' (%s)", args.pkg,
                strerror(errno));
            goto error;
        }

        char waitKey[KEY_128BIT];
        if (share(waitKey, sizeof(waitKey), wait, CLOCKS_PER_SEC) == ERR)
        {
            close(wait);
            snprintf(ctx->result, sizeof(ctx->result), "error due to wait share failure for '%s' (%s)", args.pkg,
                strerror(errno));
            goto error;
        }
        close(wait);

        snprintf(ctx->result, sizeof(ctx->result), "foreground %s", waitKey);
    }
    else
    {
        if (swrite(ctl, "close 0 -1") == ERR)
        {
            snprintf(ctx->result, sizeof(ctx->result), "error due to close failure for '%s' (%s)", args.pkg,
                strerror(errno));
            goto error;
        }

        snprintf(ctx->result, sizeof(ctx->result), "background");
    }

    if (swrite(ctl, "start") == ERR)
    {
        snprintf(ctx->result, sizeof(ctx->result), "error due to start failure for '%s' (%s)", args.pkg,
            strerror(errno));
        goto error;
    }

    goto cleanup;
error:
    if (pid != ERR)
    {
        kill(pid);
    }
cleanup:
    for (int i = 0; i < 3; i++)
    {
        if (args.stdio[i] != FD_NONE)
        {
            close(args.stdio[i]);
        }
    }
    if (args.group != FD_NONE)
    {
        close(args.group);
    }
    if (args.namespace != FD_NONE)
    {
        close(args.namespace);
    }
    if (ctl != FD_NONE)
    {
        close(ctl);
    }
}

int main(void)
{
    /// @todo Use nonblocking sockets to avoid hanging on accept or read, or just wait until we have filesystem servers
    /// and do that instead.

    char* id = sreadfile("/net/local/seqpacket");
    if (id == NULL)
    {
        printf("pkgd: failed to open local seqpacket socket (%s)\n", strerror(errno));
        abort();
    }

    if (swritefile(F("/net/local/%s/ctl", id), "bind pkgspawn && listen") == ERR)
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

        pkg_spawn_t ctx = {0};
        if (read(client, ctx.input, sizeof(ctx.input) - 1) == ERR)
        {
            printf("pkgd: failed to read pkg (%s)\n", strerror(errno));
            close(client);
            continue;
        }

        pkg_spawn(&ctx);

        if (swrite(client, ctx.result) == ERR)
        {
            printf("pkgd: failed to write pkg (%s)\n", strerror(errno));
        }

        close(client);
    }

error:
    free(id);
    return EXIT_FAILURE;
}