#include "manifest.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/argsplit.h>
#include <sys/defs.h>
#include <sys/fs.h>
#include <sys/proc.h>

/**
 * @brief Box Daemon.
 * @defgroup programs_boxd Box Daemon
 * @ingroup programs
 *
 * The box daemon is responsible for spawning and managing boxes.
 *
 * ## Spawning Boxes
 *
 * To spawn a box a request should be sent to the "boxspawn" socket in the format:
 *
 * ```
 * [key=value ...] -- <box_name> [arg1 arg2 ...]
 * ```
 *
 * Where the following values can be specified:
 * - `stdin`: A shared file descriptor to use as standard input.
 * - `stdout`: A shared file descriptor to use as standard output.
 * - `stderr`: A shared file descriptor to use as standard error.
 * - `group`: A shared file descriptor to use as the process group (`/proc/[pid]/group`)
 * - `namespace`: A shared file descriptor to use as the process namespace (`/proc/[pid]/ns`).
 *
 * @note The `stdin`, `stdout`, `stderr` and `group` values will only be used if the box is a foreground box,
 * meanwhile the `namespace` will only be used if the box uses the `inherit` sandbox profile.
 *
 * @todo Implement group and namespace specification for foreground boxes and the inherit profile.
 *
 * The "boxspawn" socket will send a response in the format:
 *
 * ```
 * <background|foreground [key]|error [msg]>
 * ```
 *
 * On success, the response will either contain `background` if the box is a background box, or `foreground`
 * followed by a key for the boxes `/proc/[pid]/wait` file if the box is a foreground box.
 *
 * On failure, the response will contain `error` followed by an error message.
 *
 * @todo Once filesystem servers are implemented the box deamon should use them instead of sockets.
 *
 * @todo Add a system for specifying environment variables.
 *
 * @{
 */

#define ARGV_MAX 512
#define BUFFER_MAX 0x1000

typedef struct
{
    char input[BUFFER_MAX];
    char result[BUFFER_MAX];
} box_spawn_t;

typedef struct
{
    const char* box;
    const char** argv;
    uint64_t argc;
    fd_t stdio[3];
    fd_t group;
    fd_t namespace;
} box_args_t;

static uint64_t box_args_parse(box_args_t* args, uint64_t argc, const char** argv, box_spawn_t* ctx)
{
    for (uint64_t i = 0; i < argc; i++)
    {
        if (strcmp(argv[i], "--") == 0)
        {
            if (i + 1 >= argc)
            {
                snprintf(ctx->result, sizeof(ctx->result), "error due to missing box name");
                return PFAIL;
            }

            args->box = argv[i + 1];
            args->argv = &argv[i + 1];
            args->argc = argc - (i + 1);
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
            if (IS_ERR(claim(&args->stdio[STDIN_FILENO], value)))
            {
                snprintf(ctx->result, sizeof(ctx->result), "error due to invalid stdin");
                return PFAIL;
            }
        }
        else if (strcmp(key, "stdout") == 0)
        {
            if (IS_ERR(claim(&args->stdio[STDOUT_FILENO], value)))
            {
                snprintf(ctx->result, sizeof(ctx->result), "error due to invalid stdout");
                return PFAIL;
            }
        }
        else if (strcmp(key, "stderr") == 0)
        {
            if (IS_ERR(claim(&args->stdio[STDERR_FILENO], value)))
            {
                snprintf(ctx->result, sizeof(ctx->result), "error due to invalid stderr");
                return PFAIL;
            }
        }
        else if (strcmp(key, "group") == 0)
        {
            if (IS_ERR(claim(&args->group, value)))
            {
                snprintf(ctx->result, sizeof(ctx->result), "error due to invalid group");
                return PFAIL;
            }
        }
        else if (strcmp(key, "namespace") == 0)
        {
            if (IS_ERR(claim(&args->namespace, value)))
            {
                snprintf(ctx->result, sizeof(ctx->result), "error due to invalid namespace");
                return PFAIL;
            }
        }
        else
        {
            snprintf(ctx->result, sizeof(ctx->result), "error due to unknown argument '%s'", key);
            return PFAIL;
        }
    }

    if (args->box == NULL || strchr(args->box, '/') != NULL || strchr(args->box, '.') != NULL)
    {
        snprintf(ctx->result, sizeof(ctx->result), "error due to missing box name");
        return PFAIL;
    }

    return 0;
}

static void box_spawn(box_spawn_t* ctx)
{
    box_args_t args = {.box = NULL, .stdio = {FD_NONE}, .group = FD_NONE, .namespace = FD_NONE};
    fd_t ctl = FD_NONE;
    pid_t pid = PFAIL;

    char argBuffer[BUFFER_MAX];
    uint64_t argc;
    const char** argv = argsplit_buf(argBuffer, sizeof(argBuffer), ctx->input, BUFFER_MAX, &argc);
    if (argv == NULL || argc == 0)
    {
        snprintf(ctx->result, sizeof(ctx->result), "error due to invalid request");
        goto error;
    }

    if (box_args_parse(&args, argc, argv, ctx) == PFAIL)
    {
        goto error;
    }

    manifest_t manifest;
    if (manifest_parse(&manifest, F("/box/%s/manifest", args.box)) == PFAIL)
    {
        snprintf(ctx->result, sizeof(ctx->result), "error due to invalid manifest for box '%s'", args.box);
        goto error;
    }

    substitution_t substitutions[] = {
        {"BOX", F("/box/%s/", args.box)},
    };
    manifest_substitute(&manifest, substitutions, ARRAY_SIZE(substitutions));

    section_t* exec = &manifest.sections[SECTION_EXEC];
    char* bin = manifest_get_value(exec, "bin");
    if (bin == NULL)
    {
        snprintf(ctx->result, sizeof(ctx->result), "error due to manifest of '%s' missing 'bin' entry", args.box);
        goto error;
    }

    uint64_t priority = manifest_get_integer(exec, "priority");
    if (priority == PFAIL)
    {
        snprintf(ctx->result, sizeof(ctx->result), "error due to manifest of '%s' missing 'priority' entry", args.box);
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
            args.box);
        goto error;
    }

    args.argv[0] = bin;
    if (IS_ERR(spawn(args.argv, flags, &pid)))
    {
        snprintf(ctx->result, sizeof(ctx->result), "error due to spawn failure for '%s' (%s)", args.box,
            strerror(errno));
        goto error;
    }

    if (IS_ERR(writefiles(F("/proc/%llu/prio", pid), F("%llu", priority))))
    {
        snprintf(ctx->result, sizeof(ctx->result), "error due to priority failure for '%s' (%s)", args.box,
            strerror(errno));
        goto error;
    }

    section_t* env = &manifest.sections[SECTION_ENV];
    for (uint64_t i = 0; i < env->amount; i++)
    {
        if (IS_ERR(writefiles(F("/proc/%llu/env/%s:cw", pid, env->entries[i].key), env->entries[i].value)))
        {
            snprintf(ctx->result, sizeof(ctx->result), "error due to env var failure for '%s' (%s)", args.box,
                strerror(errno));
            goto error;
        }
    }

    if (IS_ERR(open(&ctl, F("/proc/%llu/ctl", pid))))
    {
        snprintf(ctx->result, sizeof(ctx->result), "error due to ctl open failure for '%s' (%s)", args.box,
            strerror(errno));
        goto error;
    }

    if (shouldInheritNamespace)
    {
        if (IS_ERR(writes(ctl, F("setns %llu", args.namespace), NULL)))
        {
            snprintf(ctx->result, sizeof(ctx->result), "error due to setns failure for '%s' (%s)", args.box,
                strerror(errno));
            goto error;
        }
    }
    else
    {
        if (IS_ERR(writes(ctl, "mount /:Lrwx /sys/fs/tmpfs", NULL)))
        {
            snprintf(ctx->result, sizeof(ctx->result), "error due to root mount failure for '%s' (%s)", args.box,
                strerror(errno));
            goto error;
        }
    }

    section_t* namespace = &manifest.sections[SECTION_NAMESPACE];
    for (uint64_t i = 0; i < namespace->amount; i++)
    {
        char* key = namespace->entries[i].key;
        char* value = namespace->entries[i].value;

        if (IS_ERR(writes(ctl, F("touch %s:rwcp && bind %s %s", key, key, value), NULL)))
        {
            printf("boxd: failed to bind '%s' to '%s' (%s)\n", key, value, strerror(errno));
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

            if (IS_ERR(writes(ctl, F("dup %llu %llu", args.stdio[i], i), NULL)))
            {
                snprintf(ctx->result, sizeof(ctx->result), "error due to dup failure for '%s' (%s)", args.box,
                    strerror(errno));
                goto error;
            }
        }

        if (IS_ERR(writes(ctl, F("setgroup %llu", args.group), NULL)))
        {
            snprintf(ctx->result, sizeof(ctx->result), "error due to setns failure for '%s' (%s)", args.box,
                strerror(errno));
            goto error;
        }

        if (IS_ERR(writes(ctl, "close 3 -1", NULL)))
        {
            snprintf(ctx->result, sizeof(ctx->result), "error due to close failure for '%s' (%s)", args.box,
                strerror(errno));
            goto error;
        }

        fd_t wait;
        if (IS_ERR(open(&wait, F("/proc/%llu/wait", pid))))
        {
            snprintf(ctx->result, sizeof(ctx->result), "error due to wait open failure for '%s' (%s)", args.box,
                strerror(errno));
            goto error;
        }

        char waitKey[KEY_128BIT];
        if (IS_ERR(share(waitKey, sizeof(waitKey), wait, CLOCKS_PER_SEC)))
        {
            close(wait);
            snprintf(ctx->result, sizeof(ctx->result), "error due to wait share failure for '%s' (%s)", args.box,
                strerror(errno));
            goto error;
        }
        close(wait);

        snprintf(ctx->result, sizeof(ctx->result), "foreground %s", waitKey);
    }
    else
    {
        if (IS_ERR(writes(ctl, "close 0 -1", NULL)))
        {
            snprintf(ctx->result, sizeof(ctx->result), "error due to close failure for '%s' (%s)", args.box,
                strerror(errno));
            goto error;
        }

        snprintf(ctx->result, sizeof(ctx->result), "background");
    }

    if (IS_ERR(writes(ctl, "start", NULL)))
    {
        snprintf(ctx->result, sizeof(ctx->result), "error due to start failure for '%s' (%s)", args.box,
            strerror(errno));
        goto error;
    }

    goto cleanup;
error:
    if (pid != PFAIL)
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

    char* id;
    if (IS_ERR(readfiles(&id, "/net/local/seqpacket")))
    {
        printf("boxd: failed to open local seqpacket socket (%s)\n", strerror(errno));
        abort();
    }

    if (IS_ERR(writefiles(F("/net/local/%s/ctl", id), "bind boxspawn && listen")))
    {
        printf("boxd: failed to bind to box (%s)\n", strerror(errno));
        goto error;
    }

    printf("boxd: listening for connections...\n");
    while (1)
    {
        fd_t client;
        if (IS_ERR(open(&client, F("/net/local/%s/accept", id))))
        {
            printf("boxd: failed to accept connection (%s)\n", strerror(errno));
            goto error;
        }

        box_spawn_t ctx = {0};
        if (IS_ERR(read(client, ctx.input, sizeof(ctx.input) - 1, NULL)))
        {
            printf("boxd: failed to read request (%s)\n", strerror(errno));
            close(client);
            continue;
        }

        box_spawn(&ctx);

        if (IS_ERR(writes(client, ctx.result, NULL)))
        {
            printf("boxd: failed to write response (%s)\n", strerror(errno));
        }

        close(client);
    }

error:
    free(id);
    return EXIT_FAILURE;
}