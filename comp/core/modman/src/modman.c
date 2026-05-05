#include <libc/comp.h>
#include <libc/defs.h>
#include <libc/fs.h>
#include <libc/io.h>
#include <libc/list.h>
#include <libc/map.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char announcement[PAGE_SIZE];

typedef struct module
{
    map_entry_t mapEntry;
    char name[MAX_NAME];
} module_t;

typedef struct
{
    list_entry_t entry;
    comp_dependency_t* deps;
    size_t count;
    size_t current;
    char deviceType[MAX_NAME];
    char deviceName[MAX_PATH];
} module_loader_t;

static bool module_cmp(map_entry_t* entry, const void* key)
{
    module_t* mod = CONTAINER_OF(entry, module_t, mapEntry);
    return strcmp(mod->name, (const char*)key) == 0;
}

static list_t deferred = LIST_CREATE(deferred);
static MAP_CREATE(loaded, 64, module_cmp);

static bool module_is_loaded(const char* name)
{
    return map_find(&loaded, name, hash_string(name)) != NULL;
}

static void module_mark_loaded(const char* name)
{
    module_t* mod = malloc(sizeof(module_t));
    if (mod != NULL)
    {
        map_entry_init(&mod->mapEntry);
        strncpy(mod->name, name, MAX_NAME - 1);
        mod->name[MAX_NAME - 1] = '\0';
        map_insert(&loaded, &mod->mapEntry, hash_string(name));
    }
}

static void module_loader_free(module_loader_t* loader)
{
    comp_dependencies_free(loader->deps, loader->count);
    free(loader);
}

static status_t module_load_loop(module_loader_t* loader)
{
    for (; loader->current < loader->count; loader->current++)
    {
        bool isMain = loader->current == loader->count - 1;

        comp_dependency_t* dep = &loader->deps[loader->current];

        printf("  %s %u.%u.%u\n", dep->name, dep->version.major, dep->version.minor, dep->version.patch);
        scon_item_t* componentList = scon_find(scon_root(dep->scon), "component");
        if (!scon_is_list(componentList))
        {
            continue;
        }

        scon_item_t* moduleList = scon_find(componentList, "module");
        if (!scon_is_list(moduleList))
        {
            continue;
        }

        scon_item_t* module = scon_get(moduleList, 1);
        if (!scon_is_atom(module))
        {
            printf("  %s: module list entry is not an atom\n", dep->name);
            continue;
        }

        const char* moduleBinaryPath;
        size_t moduleBinaryPathLen;
        scon_atom_get(module, &moduleBinaryPath, &moduleBinaryPathLen);

        if (module_is_loaded(dep->name))
        {
            if (!isMain)
            {
                continue;
            }

            iovec_t vecs[2] = {{.base = (void*)loader->deviceType, .length = strlen(loader->deviceType) + 1},
                {.base = (void*)loader->deviceName, .length = strlen(loader->deviceName) + 1}};

            char attachPath[MAX_PATH];
            snprintf(attachPath, sizeof(attachPath), "/sys/mod/instances/%s/attach:write", dep->name);

            status_t status = iowritep(FDCWD, FDROOT, attachPath, vecs, 2, 0, NULL);
            if (IS_ERR(status))
            {
                printf("  %s: failed to attach to module %Y\n", dep->name, status);
                return status;
            }
            if (IS_CODE(status, DEFERRED))
            {
                list_push_back(&deferred, &loader->entry);
                return status;
            }
            continue;
        }

        fd_t fd;
        status_t status = iowalk(FDCWD, FDROOT,
            IOFMT("/comp/%s/%u.%u.%u/%.*s:read", dep->name, dep->version.major, dep->version.minor, dep->version.patch,
                (int)moduleBinaryPathLen, moduleBinaryPath),
            &fd);
        if (IS_ERR(status))
        {
            printf("  %s: failed to open module %Y\n", dep->name, status);
            continue;
        }

        size_t size = 0;
        status = ioattr(fd, FILE_GET_SIZE, &size);
        if (IS_ERR(status))
        {
            printf("  %s: failed to get module size %Y\n", dep->name, status);
            iodrop(fd);
            continue;
        }

        void* addr = NULL;
        status = iomap(fd, &addr, size, 0, IOMAP_READ);
        iodrop(fd);
        if (IS_ERR(status))
        {
            printf("  %s: failed to map module %Y\n", dep->name, status);
            continue;
        }

        const char* attachType = isMain ? loader->deviceType : "-";
        const char* attachName = isMain ? loader->deviceName : "-";

        iovec_t vecs[4] = {{.base = (void*)dep->name, .length = strlen(dep->name) + 1},
            {.base = (void*)attachType, .length = strlen(attachType) + 1},
            {.base = (void*)attachName, .length = strlen(attachName) + 1}, {.base = addr, .length = size}};

        status = iowritep(FDCWD, FDROOT, "/sys/mod/load:write", vecs, 4, 0, NULL);
        iounmap(addr, size);

        if (IS_ERR(status))
        {
            printf("  %s: failed to load module %Y\n", dep->name, status);
            return status;
        }

        module_mark_loaded(dep->name);

        if (IS_CODE(status, DEFERRED))
        {
            list_push_back(&deferred, &loader->entry);
            return status;
        }
    }

    return OK;
}

static status_t module_load(const char* name, const char* version, const char* deviceType, const char* deviceName)
{
    if (name == NULL || version == NULL || deviceType == NULL || deviceName == NULL)
    {
        return ERR(USER, INVAL);
    }

    module_loader_t* loader = malloc(sizeof(module_loader_t));
    if (loader == NULL)
    {
        return ERR(USER, NOMEM);
    }

    char errorBuf[256];
    status_t status = comp_dependencies_get(name, version, &loader->deps, &loader->count, errorBuf, sizeof(errorBuf));
    if (IS_ERR(status))
    {
        printf("modman: failed to get dependencies for %s %s: %s %Y\n", name, version, errorBuf, status);
        free(loader);
        return status;
    }
    loader->current = 0;
    list_entry_init(&loader->entry);
    strncpy(loader->deviceType, deviceType, MAX_NAME - 1);
    loader->deviceType[MAX_NAME - 1] = '\0';
    strncpy(loader->deviceName, deviceName, MAX_PATH - 1);
    loader->deviceName[MAX_PATH - 1] = '\0';

    status = module_load_loop(loader);
    if (IS_ERR(status) || !IS_CODE(status, DEFERRED))
    {
        module_loader_free(loader);
    }

    return status;
}

static status_t module_attach(const char* type, const char* compat, const char* name)
{
    char* symlinks = NULL;
    size_t symlinksLen = 0;
    const char* targetType = type;
    status_t status = ioloadp(FDCWD, FDROOT, IOFMT("/comp/.index/device/%s", targetType), &symlinks, &symlinksLen);
    if (IS_ERR(status) || symlinksLen == 0)
    {
        if (symlinks != NULL)
        {
            free(symlinks);
            symlinks = NULL;
        }

        if (compat != NULL)
        {
            targetType = compat;
            status = ioloadp(FDCWD, FDROOT, IOFMT("/comp/.index/device/%s", targetType), &symlinks, &symlinksLen);
        }

        if (IS_ERR(status) || symlinksLen == 0)
        {
            if (symlinks != NULL)
            {
                free(symlinks);
                symlinks = NULL;
            }
        }
    }

    if (symlinks != NULL && symlinksLen > 0)
    {
        const char* symlink = symlinks;
        while (symlink < symlinks + symlinksLen)
        {
            size_t len = strlen(symlink);
            if (len == 0 || symlink[0] == '.')
            {
                symlink += len + 1;
                continue;
            }

            static char path[MAX_PATH];
            size_t pathLen;
            status = ioreadp(FDCWD, FDROOT, IOFMT("/comp/.index/device/%s/%s:read:nofollow", targetType, symlink),
                IOBUF(path, sizeof(path) - 1), 0, &pathLen);
            if (IS_ERR(status))
            {
                printf("modman: failed to read symlink %s %Y\n", symlink, status);
                symlink += len + 1;
                continue;
            }
            path[pathLen] = '\0';

            static char compName[MAX_PATH];
            static char compVersion[MAX_PATH];
            if (sscanf(path, "/comp/%[^/]/%s", compName, compVersion) != 2)
            {
                printf("modman: failed to parse path %s\n", path);
                symlink += len + 1;
                continue;
            }

            printf("modman: loading module %s %s\n", compName, compVersion);

            status = module_load(compName, compVersion, targetType, name);
            if (IS_ERR(status))
            {
                printf("modman: failed to load module %s %s %Y\n", compName, compVersion, status);
            }

            symlink += len + 1;
        }

        free(symlinks);
    }

    list_t local = LIST_CREATE(local);

    list_splice(&local, &deferred);

    while (!list_is_empty(&local))
    {
        module_loader_t* loader = CONTAINER_OF(list_pop_front(&local), module_loader_t, entry);

        status_t status = module_load_loop(loader);
        if (IS_ERR(status))
        {
            printf("modman: failed to load deferred module %s %Y\n", loader->deps[loader->current].name, status);
            module_loader_free(loader);
            continue;
        }

        if (!IS_CODE(status, DEFERRED))
        {
            module_loader_free(loader);
        }
    }
    return OK;
}

int main(int argc, char** argv)
{
    UNUSED(argc);
    UNUSED(argv);

    fd_t announce;
    status_t status = iowalk(FDCWD, FDROOT, "/dev/announce", &announce);
    if (IS_ERR(status))
    {
        printf("failed to open /dev/announce %Y\n", status);
        return EXIT_FAILURE;
    }

    status = module_attach("BOOT_ALWAYS", NULL, "BOOT_ALWAYS");
    if (IS_ERR(status))
    {
        printf("failed to announce BOOT_ALWAYS %Y\n", status);
        iodrop(announce);
        return EXIT_FAILURE;
    }

    while (true)
    {
        size_t bytesRead;
        status = ioread(announce, IOBUF(announcement, sizeof(announcement) - 1), 0, &bytesRead);
        if (IS_ERR(status))
        {
            return EXIT_FAILURE;
        }

        announcement[bytesRead] = '\0';

        char* line = announcement;
        while (line < announcement + bytesRead && *line != '\0')
        {
            char* nextLine = strchr(line, '\n');
            if (nextLine != NULL)
            {
                *nextLine = '\0';
                nextLine++;
            }
            else
            {
                nextLine = announcement + bytesRead;
            }

            char* timestamp = line;
            char* change = strchr(timestamp, ' ');
            char* type = change != NULL ? strchr(change + 1, ' ') : NULL;
            char* compat = type != NULL ? strchr(type + 1, ' ') : NULL;
            char* name = compat != NULL ? strchr(compat + 1, ' ') : NULL;

            if (timestamp != NULL && change != NULL && type != NULL && compat != NULL && name != NULL)
            {
                *change++ = '\0';
                *type++ = '\0';
                *compat++ = '\0';
                *name++ = '\0';

                if (strcmp(change, "attach") == 0)
                {
                    status = module_attach(type, strcmp(compat, "-") == 0 ? NULL : compat, name);
                    if (IS_ERR(status))
                    {
                        printf("modman: failed to attach %s %s %s %s %Y\n", timestamp, change, type, compat, name,
                            status);
                    }
                }
            }

            line = nextLine;
        }
    }

    return 0;
}
