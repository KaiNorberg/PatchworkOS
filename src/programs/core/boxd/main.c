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
            status_t status = claim(&args->stdio[STDIN_FILENO], value);
            if (IS_ERR(status))
            {
                snprintf(ctx->result, sizeof(ctx->result), IOFMT("error due to invalid stdin %Y", status));
                return PFAIL;
            }
        }
        else if (strcmp(key, "stdout") == 0)
        {
            status_t status = claim(&args->stdio[STDOUT_FILENO], value);
            if (IS_ERR(status))
            {
                snprintf(ctx->result, sizeof(ctx->result), IOFMT("error due to invalid stdout %Y", status));
                return PFAIL;
            }
        }
        else if (strcmp(key, "stderr") == 0)
        {
            status_t status = claim(&args->stdio[STDERR_FILENO], value);
            if (IS_ERR(status))
            {
                snprintf(ctx->result, sizeof(ctx->result), IOFMT("error due to invalid stderr %Y", status));
                return PFAIL;
            }
        }
        else if (strcmp(key, "group") == 0)
        {
            status_t status = claim(&args->group, value);
            if (IS_ERR(status))
            {
                printf("boxd: failed to claim group '%s' %Y\n", value, status);
                snprintf(ctx->result, sizeof(ctx->result), IOFMT("error due to invalid group %Y", status));
                return PFAIL;
            }
        }
        else if (strcmp(key, "namespace") == 0)
        {
            status_t status = claim(&args->namespace, value);
            if (IS_ERR(status))
            {
                snprintf(ctx->result, sizeof(ctx->result), IOFMT("error due to invalid namespace %Y", status));
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
    box_args_t args = {.box = NULL, .stdio = {FDNONE}, .group = FDNONE, .namespace = FDNONE};
    fd_t ctl = FDNONE;
    proc_t pid = PFAIL;
    status_t status;

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
    if (manifest_parse(&manifest, IOFMT("/box/%s/manifest", args.box)) == PFAIL)
    {
        snprintf(ctx->result, sizeof(ctx->result), "error due to invalid manifest for box '%s'", args.box);
        goto error;
    }

    substitution_t substitutions[] = {
        {"BOX", IOFMT("/box/%s/", args.box)},
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

    proc_flags_t flags = PROC_SUSPEND | PROC_EMPTY_ENV | PROC_EMPTY_CWD | PROC_EMPTY_GROUP;
    if (strcmp(profile, "empty") == 0)
    {
        flags |= PROC_EMPTY_NS;
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
    status = proc_create(args.argv, flags, &pid);
    if (IS_ERR(status))
    {
        snprintf(ctx->result, sizeof(ctx->result), "error due to spawn failure for '%s' %Y", args.box, status);
        goto error;
    }

    status = writefiles(IOFMT("/proc/%llu/prio", pid), IOFMT("%llu", priority));
    if (IS_ERR(status))
    {
        snprintf(ctx->result, sizeof(ctx->result), "error due to priority failure for '%s' %Y", args.box, status);
        goto error;
    }

    section_t* env = &manifest.sections[SECTION_ENV];
    for (uint64_t i = 0; i < env->amount; i++)
    {
        status = writefiles(IOFMT("/proc/%llu/env/%s:cw", pid, env->entries[i].key), env->entries[i].value);
        if (IS_ERR(status))
        {
            snprintf(ctx->result, sizeof(ctx->result), "error due to env var failure for '%s' %Y", args.box, status);
            goto error;
        }
    }

    status = open(&ctl, IOFMT("/proc/%llu/ctl", pid));
    if (IS_ERR(status))
    {
        snprintf(ctx->result, sizeof(ctx->result), "error due to ctl open failure for '%s' %Y", args.box, status);
        goto error;
    }

    if (shouldInheritNamespace)
    {
        status = writes(ctl, IOFMT("setns %llu", args.namespace), NULL);
        if (IS_ERR(status))
        {
            snprintf(ctx->result, sizeof(ctx->result), "error due to setns failure for '%s' %Y", args.box, status);
            goto error;
        }
    }
    else
    {
        status = writes(ctl, "mount /:Lrwx /sys/fs/tmpfs", NULL);
        if (IS_ERR(status))
        {
            snprintf(ctx->result, sizeof(ctx->result), "error due to root mount failure for '%s' %Y", args.box, status);
            goto error;
        }
    }

    section_t* namespace = &manifest.sections[SECTION_NAMESPACE];
    for (uint64_t i = 0; i < namespace->amount; i++)
    {
        char* key = namespace->entries[i].key;
        char* value = namespace->entries[i].value;

        if (value[0] == '\0')
        {
            status = writes(ctl, IOFMT("touch %s", key), NULL);
            if (IS_ERR(status))
            {
                printf("boxd: failed to touch '%s' %Y\n", key, status);
                goto error;
            }
            continue;
        }

        status = writes(ctl, IOFMT("touch %s:rwcp && bind %s %s", key, key, value), NULL);
        if (IS_ERR(status))
        {
            printf("boxd: failed to bind '%s' to '%s' %Y\n", key, value, status);
            goto error;
        }
    }

    if (isForeground)
    {
        for (uint8_t i = 0; i < 3; i++)
        {
            if (args.stdio[i] == FDNONE)
            {
                continue;
            }

            status = writes(ctl, IOFMT("dup %llu %llu", args.stdio[i], i), NULL);
            if (IS_ERR(status))
            {
                snprintf(ctx->result, sizeof(ctx->result), "error due to dup failure for '%s' %Y", args.box, status);
                goto error;
            }
        }

        status = writes(ctl, IOFMT("setgroup %llu", args.group), NULL);
        if (IS_ERR(status))
        {
            snprintf(ctx->result, sizeof(ctx->result), "error due to setns failure for '%s' %Y", args.box, status);
            goto error;
        }

        status = writes(ctl, "close 3 -1", NULL);
        if (IS_ERR(status))
        {
            snprintf(ctx->result, sizeof(ctx->result), "error due to close failure for '%s' %Y", args.box, status);
            goto error;
        }

        fd_t wait;
        status = open(&wait, IOFMT("/proc/%llu/wait", pid));
        if (IS_ERR(status))
        {
            snprintf(ctx->result, sizeof(ctx->result), "error due to wait open failure for '%s' %Y", args.box, status);
            goto error;
        }

        char waitKey[KEY_128BIT];
        status = share(waitKey, sizeof(waitKey), wait, CLOCKS_PER_SEC);
        if (IS_ERR(status))
        {
            close(wait);
            snprintf(ctx->result, sizeof(ctx->result), "error due to wait share failure for '%s' %Y", args.box, status);
            goto error;
        }
        close(wait);

        snprintf(ctx->result, sizeof(ctx->result), "foreground %s", waitKey);
    }
    else
    {
        status = writes(ctl, "close 0 -1", NULL);
        if (IS_ERR(status))
        {
            snprintf(ctx->result, sizeof(ctx->result), "error due to close failure for '%s' %Y", args.box, status);
            goto error;
        }

        snprintf(ctx->result, sizeof(ctx->result), "background");
    }

    status = writes(ctl, "start", NULL);
    if (IS_ERR(status))
    {
        snprintf(ctx->result, sizeof(ctx->result), "error due to start failure for '%s' %Y", args.box, status);
        goto error;
    }

    goto cleanup;
error:
    if (pid != PFAIL)
    {
        proc_kill(pid);
    }
cleanup:
    for (int i = 0; i < 3; i++)
    {
        if (args.stdio[i] != FDNONE)
        {
            close(args.stdio[i]);
        }
    }
    if (args.group != FDNONE)
    {
        close(args.group);
    }
    if (args.namespace != FDNONE)
    {
        close(args.namespace);
    }
    if (ctl != FDNONE)
    {
        close(ctl);
    }
}

int main(void)
{
    /// @todo Use nonblocking sockets to avoid hanging on accept or ioread, or just wait until we have filesystem
    /// servers and do that instead.

    char* id;
    status_t status = readfiles(&id, "/net/local/seqpacket");
    if (IS_ERR(status))
    {
        printf("boxd: failed to open local seqpacket socket %Y\n", status);
        abort();
    }

    status = writefiles(IOFMT("/net/local/%s/ctl", id), "bind boxspawn && listen");
    if (IS_ERR(status))
    {
        printf("boxd: failed to bind to box %Y\n", status);
        goto error;
    }

    printf("boxd: listening for connections...\n");
    while (1)
    {
        fd_t client;
        status = open(&client, IOFMT("/net/local/%s/accept", id));
        if (IS_ERR(status))
        {
            printf("boxd: failed to accept connection %Y\n", status);
            goto error;
        }

        box_spawn_t ctx = {0};
        status = ioread(client, ctx.input, sizeof(ctx.input) - 1, IOCUR, NULL);
        if (IS_ERR(status))
        {
            printf("boxd: failed to ioread request %Y\n", status);
            close(client);
            continue;
        }

        box_spawn(&ctx);

        status = writes(client, ctx.result, NULL);
        if (IS_ERR(status))
        {
            printf("boxd: failed to iowrite response %Y\n", status);
        }

        close(client);
    }

error:
    free(id);
    return EXIT_FAILURE;
}