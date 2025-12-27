#include "manifest.h"

#include <stdio.h>
#include <sys/io.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/proc.h>

/**
 * @brief Package Daemon.
 * @defgroup programs_pkgd Package Daemon
 * @ingroup programs
 * 
 */

#define ARGV_MAX 512

static uint64_t pkg_spawn(const char* pkg)
{
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

    char* bin = manifest_get_value(&manifest.sections[SECTION_EXEC], "bin");
    if (bin == NULL)
    {
        printf("pkgd: manifest of '%s' missing 'bin' entry\n", pkg);
        return ERR;
    }

    if (bin[0] == '/')
    {
        printf("pkgd: manifest of '%s' has invalid 'bin' entry (must be relative path)\n", pkg);
        return ERR;
    }

    uint64_t priority = manifest_get_integer(&manifest.sections[SECTION_EXEC], "priority");
    if (priority == ERR)
    {
        printf("pkgd: manifest of '%s' invalid or missing 'priority' entry\n", pkg);
        return ERR;
    }

    printf("pkgd: spawning '%s'\n", bin);
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

        char* pkg = sread(client);
        if (pkg == NULL)
        {
            printf("pkgd: failed to read pkg (%s)\n", strerror(errno));
            close(client);
            continue;
        }

        if (pkg_spawn(pkg) == ERR)
        {
            printf("pkgd: failed to spawn pkg (%s)\n", strerror(errno));
            free(pkg);
            close(client);
            continue;
        }

        free(pkg);
        close(client);
    }

error:
    free(id);
    return EXIT_FAILURE;
}