#include <ctype.h>
#include <stdint.h>
#include <string.h>
#include <sys/comp.h>
#include <sys/fs.h>
#include <sys/map.h>
#include <sys/proc.h>
#include <sys/scon.h>

typedef struct
{
    union {
        struct
        {
            uint32_t major;
            uint32_t minor;
            uint32_t patch;
        };
        uint32_t array[3];
    };
} comp_version_t;

typedef struct
{
    list_entry_t entry;
    map_entry_t mapEntry;
    char name[MAX_NAME];
    comp_version_t version;
    char* manifest;
    size_t manifestLength;
    scon_t scon;
    bool processed;
} comp_req_t;

#define COMP_VERSION_BASE (1ULL << 20)

typedef struct
{
    list_t reqs;
    MAP_DEFINE(map, 16);
} comp_loader_t;

#define COMP_CAP_MAX 64

typedef struct
{
    char* path;
    size_t length;
} comp_cap_t;

typedef struct
{
    comp_cap_t caps[COMP_CAP_MAX];
    size_t count;
} comp_cap_set_t;

typedef enum
{
    COMP_DIR_BIN,
    COMP_DIR_LIB,
    COMP_DIR_INCLUDE,
    COMP_DIR_DATA,
    COMP_DIR_CFG,
    COMP_DIR_MAX,
} comp_dirs_t;

typedef struct
{
    fd_t sources[COMP_CAP_MAX];
    size_t count;
} comp_union_t;

static bool comp_req_compare(map_entry_t* entry, const void* key)
{
    comp_req_t* req = CONTAINER_OF(entry, comp_req_t, mapEntry);
    return strcmp(req->name, (const char*)key) == 0;
}

static void comp_loader_init(comp_loader_t* loader)
{
    list_init(&loader->reqs);
    MAP_DEFINE_INIT(loader->map, comp_req_compare);
}

static void comp_loader_deinit(comp_loader_t* loader)
{
    comp_req_t* req;
    comp_req_t* temp;
    LIST_FOR_EACH_SAFE(req, temp, &loader->reqs, entry)
    {
        if (req->manifest != NULL)
        {
            scon_deinit(&req->scon);
            free(req->manifest);
        }
        list_remove(&req->entry);
        free(req);
    }
}

static int64_t comp_version_compare(comp_version_t a, comp_version_t b)
{
    if (a.major != b.major)
    {
        return (int64_t)a.major - (int64_t)b.major;
    }
    if (a.minor != b.minor)
    {
        return (int64_t)a.minor - (int64_t)b.minor;
    }
    return (int64_t)a.patch - (int64_t)b.patch;
}

static status_t comp_version_parse(const char* version, size_t versionLen, comp_version_t* out)
{
    if (version == NULL || out == NULL)
    {
        return ERR(LIBSTD, INVAL);
    }

    const char* p = version;
    const char* end = version + versionLen;

    out->major = 0;
    out->minor = 0;
    out->patch = 0;

    for (uint64_t i = 0; i < 3; i++)
    {
        if (p >= end || !isdigit(*p))
        {
            return ERR(LIBSTD, INVAL);
        }

        while (p < end && *p != '.')
        {
            if (!isdigit(*p))
            {
                return ERR(LIBSTD, INVAL);
            }
            out->array[i] = (out->array[i] * 10) + (*p - '0');
            p++;
        }

        if (i < 2)
        {
            if (p == end || *p != '.')
            {
                return ERR(LIBSTD, INVAL);
            }
            p++; // Skip '.'
        }
    }

    if (p < end)
    {
        return ERR(LIBSTD, INVAL);
    }

    return OK;
}

static status_t comp_add(comp_loader_t* loader, const char* name, size_t nameLen, const char* version,
    size_t versionLen, comp_req_t** out)
{
    if (loader == NULL || name == NULL || nameLen >= MAX_NAME || version == NULL || out == NULL)
    {
        return ERR(LIBSTD, INVAL);
    }

    comp_req_t* req = malloc(sizeof(comp_req_t));
    if (req == NULL)
    {
        return ERR(LIBSTD, NOMEM);
    }

    memcpy(req->name, name, nameLen);
    req->name[nameLen] = '\0';

    status_t status = comp_version_parse(version, versionLen, &req->version);
    if (IS_ERR(status))
    {
        free(req);
        return status;
    }

    list_entry_init(&req->entry);
    map_entry_init(&req->mapEntry);
    list_push_back(&loader->reqs, &req->entry);
    map_insert(&loader->map, &req->mapEntry, hash_string(req->name));
    req->manifest = NULL;
    req->manifestLength = 0;
    req->processed = false;

    *out = req;
    return OK;
}

static status_t comp_find_lowest_available(comp_loader_t* loader, const char* name, comp_version_t reqVersion,
    char* out, size_t outLen, comp_version_t* outVersion)
{
    if (loader == NULL || name == NULL || out == NULL)
    {
        return ERR(LIBSTD, INVAL);
    }

    char compPath[MAX_PATH] = "/comp/";
    strcpy(compPath + 6, name);

    char* versions;
    size_t versionsLength;
    status_t status = ioloadp(FDCWD, FDROOT, compPath, &versions, &versionsLength);
    if (IS_ERR(status))
    {
        return status;
    }

    comp_version_t selected;
    selected.major = UINT32_MAX;
    selected.minor = UINT32_MAX;
    selected.patch = UINT32_MAX;
    const char* selectedP = NULL;

    char* p = versions;
    while (p < versions + versionsLength)
    {
        size_t len = strlen(p);
        if (len == 0 || p[0] == '.')
        {
            p += len + 1;
            continue;
        }

        comp_version_t version;
        status = comp_version_parse(p, len, &version);
        if (IS_ERR(status))
        {
            p += len + 1;
            continue;
        }

        if (comp_version_compare(version, reqVersion) >= 0)
        {
            if (comp_version_compare(version, selected) < 0)
            {
                selected = version;
                selectedP = p;
            }
        }

        p += len + 1;
    }

    if (selectedP == NULL)
    {
        free(versions);
        return ERR(LIBSTD, NOENT);
    }

    size_t copyLen = strlen(selectedP);
    if (copyLen >= outLen)
    {
        copyLen = outLen - 1;
    }
    memcpy(out, selectedP, copyLen);
    out[copyLen] = '\0';

    *outVersion = selected;

    free(versions);
    return OK;
}

static status_t comp_check(comp_loader_t* loader, comp_req_t* req)
{
    if (loader == NULL || req == NULL)
    {
        return ERR(LIBSTD, INVAL);
    }

    const size_t compLen = 6;
    char manifestPath[MAX_PATH] = "/comp/";
    size_t nameLength = strlen(req->name);
    memcpy(manifestPath + compLen, req->name, nameLength);
    manifestPath[compLen + nameLength] = '/';
    manifestPath[compLen + nameLength + 1] = '\0';

    comp_version_t actualVersion;
    status_t status = comp_find_lowest_available(loader, req->name, req->version,
        manifestPath + compLen + nameLength + 1, sizeof(manifestPath) - compLen - nameLength - 1 - 15, &actualVersion);
    if (IS_ERR(status))
    {
        return status;
    }

    req->version = actualVersion;
    strcat(manifestPath, "/manifest.scon");

    if (req->manifest != NULL)
    {
        scon_deinit(&req->scon);
        free(req->manifest);
    }
    req->manifest = NULL;
    req->manifestLength = 0;

    status = ioloadp(FDCWD, FDROOT, manifestPath, &req->manifest, &req->manifestLength);
    if (IS_ERR(status))
    {
        return status;
    }

    status = scon_init(&req->scon, req->manifest, req->manifestLength);
    if (IS_ERR(status))
    {
        return status;
    }

    scon_ref_t compList = scon_find(scon_root(&req->scon), "component");
    if (!scon_is_list(compList))
    {
        return ERR(LIBSTD, INVALSCON);
    }

    scon_ref_t dependencies = scon_find(compList, "dependencies");
    if (!scon_is_list(dependencies))
    {
        req->processed = true;
        return OK;
    }

    scon_ref_t dep;
    SCON_FOR_EACH(dep, dependencies, 1)
    {
        if (!scon_is_list(dep))
        {
            continue;
        }

        scon_ref_t nameAtom = scon_first(dep);
        if (!scon_is_atom(nameAtom))
        {
            continue;
        }

        scon_ref_t versionAtom = scon_next(nameAtom);
        if (!scon_is_atom(versionAtom))
        {
            continue;
        }

        const char* nameStr;
        size_t nameLen;
        if (IS_ERR(scon_atom_get(nameAtom, &nameStr, &nameLen)))
        {
            continue;
        }

        const char* versionStr;
        size_t versionLen;
        if (IS_ERR(scon_atom_get(versionAtom, &versionStr, &versionLen)))
        {
            continue;
        }

        char depName[MAX_NAME];
        if (nameLen >= MAX_NAME)
        {
            continue;
        }
        memcpy(depName, nameStr, nameLen);
        depName[nameLen] = '\0';

        map_entry_t* entry = map_find(&loader->map, depName, hash_string(depName));
        comp_req_t* depReq = entry ? CONTAINER_OF(entry, comp_req_t, mapEntry) : NULL;

        if (depReq == NULL)
        {
            status = comp_add(loader, depName, nameLen, versionStr, versionLen, &depReq);
            if (IS_ERR(status))
            {
                return status;
            }
        }
        else
        {
            comp_version_t reqDepVersion;
            if (IS_ERR(comp_version_parse(versionStr, versionLen, &reqDepVersion)))
            {
                continue;
            }

            if (comp_version_compare(depReq->version, reqDepVersion) < 0)
            {
                depReq->version = reqDepVersion;
                depReq->processed = false;
            }
        }
    }

    req->processed = true;
    return OK;
}

static status_t comp_load_dependencies(comp_loader_t* loader)
{
    if (loader == NULL)
    {
        return ERR(LIBSTD, INVAL);
    }

    bool resolving = true;
    while (resolving)
    {
        resolving = false;
        comp_req_t* req;
        LIST_FOR_EACH(req, &loader->reqs, entry)
        {
            if (req->processed)
            {
                continue;
            }

            status_t status = comp_check(loader, req);
            if (IS_ERR(status))
            {
                return status;
            }
            resolving = true;
            break;
        }
    }

    return OK;
}

static void comp_mark_reachable(comp_loader_t* loader, comp_req_t* req)
{
    if (req->processed)
    {
        return;
    }

    req->processed = true;

    if (req->manifest == NULL)
    {
        return;
    }

    scon_ref_t compList = scon_find(scon_root(&req->scon), "component");
    if (!scon_is_list(compList))
    {
        return;
    }

    scon_ref_t dependencies = scon_find(compList, "dependencies");
    if (!scon_is_list(dependencies))
    {
        return;
    }

    scon_ref_t dep;
    SCON_FOR_EACH(dep, dependencies, 1)
    {
        if (!scon_is_list(dep))
        {
            continue;
        }

        scon_ref_t nameAtom = scon_first(dep);
        if (!scon_is_atom(nameAtom))
        {
            continue;
        }

        const char* nameStr;
        size_t nameLen;
        if (IS_ERR(scon_atom_get(nameAtom, &nameStr, &nameLen)))
        {
            continue;
        }

        char depName[MAX_NAME];
        if (nameLen >= MAX_NAME)
        {
            continue;
        }
        memcpy(depName, nameStr, nameLen);
        depName[nameLen] = '\0';

        map_entry_t* entry = map_find(&loader->map, depName, hash_string(depName));
        if (entry != NULL)
        {
            comp_req_t* depReq = CONTAINER_OF(entry, comp_req_t, mapEntry);
            comp_mark_reachable(loader, depReq);
        }
    }
}

static status_t comp_cull_orphans(comp_loader_t* loader, comp_req_t* main)
{
    if (loader == NULL || main == NULL)
    {
        return ERR(LIBSTD, INVAL);
    }

    comp_req_t* req;
    LIST_FOR_EACH(req, &loader->reqs, entry)
    {
        req->processed = false;
    }

    comp_mark_reachable(loader, main);

    comp_req_t* temp;
    LIST_FOR_EACH_SAFE(req, temp, &loader->reqs, entry)
    {
        if (!req->processed)
        {
            list_remove(&req->entry);
            map_remove(&loader->map, &req->mapEntry, hash_string(req->name));
            if (req->manifest != NULL)
            {
                scon_deinit(&req->scon);
                free(req->manifest);
            }
            free(req);
        }
    }

    return OK;
}

static void comp_cap_set_deinit(comp_cap_set_t* set)
{
    for (size_t i = 0; i < set->count; i++)
    {
        free(set->caps[i].path);
    }
    set->count = 0;
}

static status_t comp_cap_set_add(comp_cap_set_t* set, const char* path, size_t length)
{
    if (set == NULL || path == NULL || length == 0)
    {
        return ERR(LIBSTD, INVAL);
    }

    size_t baseLen = 0;
    while (baseLen < length && path[baseLen] != ':' && path[baseLen] != '?')
    {
        baseLen++;
    }

    for (size_t i = 0; i < set->count; i++)
    {
        size_t existBaseLen = 0;
        while (existBaseLen < set->caps[i].length && set->caps[i].path[existBaseLen] != ':' &&
            set->caps[i].path[existBaseLen] != '?')
        {
            existBaseLen++;
        }

        if (baseLen != existBaseLen || memcmp(set->caps[i].path, path, baseLen) != 0)
        {
            continue;
        }

        size_t suffixLen = length - baseLen;
        if (suffixLen == 0)
        {
            return OK;
        }

        char* newPath = realloc(set->caps[i].path, set->caps[i].length + suffixLen + 1);
        if (newPath == NULL)
        {
            return ERR(LIBSTD, NOMEM);
        }

        memcpy(newPath + set->caps[i].length, path + baseLen, suffixLen);
        set->caps[i].length += suffixLen;
        newPath[set->caps[i].length] = '\0';
        set->caps[i].path = newPath;
        return OK;
    }

    if (set->count >= COMP_CAP_MAX)
    {
        return ERR(LIBSTD, NOMEM);
    }

    char* newPath = malloc(length + 1);
    if (newPath == NULL)
    {
        return ERR(LIBSTD, NOMEM);
    }

    memcpy(newPath, path, length);
    newPath[length] = '\0';

    set->caps[set->count].path = newPath;
    set->caps[set->count].length = length;
    set->count++;

    return OK;
}

static status_t comp_load_capability(comp_cap_set_t* caps, scon_ref_t cap)
{
    if (!scon_is_atom(cap))
    {
        return OK;
    }

    const char* capStr;
    size_t capLen;
    if (IS_ERR(scon_atom_get(cap, &capStr, &capLen)))
    {
        return OK;
    }

    if (capLen == 0)
    {
        return OK;
    }

    status_t status = comp_cap_set_add(caps, capStr, capLen);
    if (IS_ERR(status))
    {
        return status;
    }

    return OK;
}

static status_t comp_load_capabilities(comp_loader_t* loader, fd_t root)
{
    if (loader == NULL)
    {
        return ERR(LIBSTD, INVAL);
    }

    comp_cap_set_t caps;
    caps.count = 0;

    comp_req_t* req;
    LIST_FOR_EACH(req, &loader->reqs, entry)
    {
        if (req->manifest == NULL)
        {
            continue;
        }

        scon_ref_t compList = scon_find(scon_root(&req->scon), "component");
        if (!scon_is_list(compList))
        {
            continue;
        }

        scon_ref_t capabilities = scon_find(compList, "capabilities");
        if (!scon_is_list(capabilities))
        {
            continue;
        }

        scon_ref_t cap;
        SCON_FOR_EACH(cap, capabilities, 1)
        {
            status_t status = comp_load_capability(&caps, cap);
            if (IS_ERR(status))
            {
                comp_cap_set_deinit(&caps);
                return status;
            }
        }
    }

    status_t status = OK;
    for (size_t i = 0; i < caps.count; i++)
    {
        fd_t src;
        status = iowalk(FDCWD, FDROOT, caps.caps[i].path, &src);
        if (IS_ERR(status))
        {
            break;
        }

        file_type_t type = 0;
        status = ioattr(src, FILE_GET_TYPE, &type);
        if (IS_ERR(status))
        {
            iodrop(src);
            break;
        }

        fd_t target;
        status =
            iowalk(root, root, IOFMT("%s:%s", caps.caps[i].path, type == FILE_TYPE_DIRECTORY ? "dp" : "cp"), &target);
        if (IS_ERR(status))
        {
            iodrop(src);
            break;
        }

        status = fdbind(root, target, src);
        iodrop(target);
        iodrop(src);

        if (IS_ERR(status))
        {
            break;
        }
    }

    comp_cap_set_deinit(&caps);
    return status;
}

static status_t comp_load_union(comp_loader_t* loader, fd_t root)
{
    if (loader == NULL)
    {
        return ERR(LIBSTD, INVAL);
    }

    comp_union_t unions[COMP_DIR_MAX];
    for (uint32_t i = 0; i < COMP_DIR_MAX; i++)
    {
        unions[i].count = 0;
    }

    const char* dirNames[COMP_DIR_MAX] = {"bin", "lib", "include", "data", "cfg"};

    comp_req_t* req;
    LIST_FOR_EACH(req, &loader->reqs, entry)
    {
        if (req->manifest == NULL)
        {
            continue;
        }

        for (uint32_t i = 0; i < COMP_DIR_MAX; i++)
        {
            char path[MAX_PATH];
            snprintf(path, sizeof(path), "/comp/%s/%u.%u.%u/%s", req->name, req->version.major, req->version.minor,
                req->version.patch, dirNames[i]);

            fd_t dir;
            status_t status = iowalk(FDCWD, FDROOT, path, &dir);
            if (!IS_ERR(status))
            {
                if (unions[i].count < COMP_CAP_MAX)
                {
                    unions[i].sources[unions[i].count++] = dir;
                }
                else
                {
                    iodrop(dir);
                }
            }
        }
    }

    status_t status = OK;
    for (uint32_t i = 0; i < COMP_DIR_MAX; i++)
    {
        if (unions[i].count == 0)
        {
            continue;
        }

        fd_t target;
        status = iowalk(root, root, IOFMT("%s:dp", dirNames[i]), &target);
        if (IS_ERR(status))
        {
            break;
        }

        char concatPath[MAX_PATH] = "/sys/fs/concatfs/clone?targets=\0";
        for (uint32_t j = 0; j < unions[i].count; j++)
        {
            snprintf(concatPath + strlen(concatPath), sizeof(concatPath) - strlen(concatPath), "%llu,",
                unions[i].sources[j]);
        }
        concatPath[strlen(concatPath) - 1] = '\0';

        fd_t concatfs;
        status = iowalk(FDCWD, FDROOT, concatPath, &concatfs);
        if (IS_ERR(status))
        {
            iodrop(target);
            break;
        }

        status = fdbind(root, target, concatfs);
        iodrop(concatfs);
        iodrop(target);
        if (IS_ERR(status))
        {
            continue;
        }
    }

    for (uint32_t i = 0; i < COMP_DIR_MAX; i++)
    {
        for (uint32_t j = 0; j < unions[i].count; j++)
        {
            iodrop(unions[i].sources[j]);
        }
    }

    return status;
}

status_t comp_launch(const char* name, const char* version, const comp_options_t* opts)
{
    UNUSED(opts);

    if (name == NULL || version == NULL)
    {
        return ERR(LIBSTD, INVAL);
    }

    comp_version_t target;
    status_t status = comp_version_parse(version, strlen(version), &target);
    if (IS_ERR(status))
    {
        return status;
    }

    comp_loader_t loader;
    comp_loader_init(&loader);

    comp_req_t* main = NULL;
    status = comp_add(&loader, name, strlen(name), version, strlen(version), &main);
    if (IS_ERR(status))
    {
        comp_loader_deinit(&loader);
        return status;
    }

    status = comp_load_dependencies(&loader);
    if (IS_ERR(status))
    {
        comp_loader_deinit(&loader);
        return status;
    }

    scon_ref_t compList = scon_find(scon_root(&main->scon), "component");
    if (!scon_is_list(compList))
    {
        comp_loader_deinit(&loader);
        return ERR(LIBSTD, INVALSCON);
    }

    scon_ref_t launchList = scon_find(compList, "launch");
    if (!scon_is_list(launchList))
    {
        comp_loader_deinit(&loader);
        return ERR(LIBSTD, INVALSCON);
    }

    scon_ref_t launch = scon_get(launchList, 1);
    if (!scon_is_atom(launch))
    {
        comp_loader_deinit(&loader);
        return ERR(LIBSTD, INVALSCON);
    }

    const char* launchPath;
    size_t launchPathLen;
    status = scon_atom_get(launch, &launchPath, &launchPathLen);
    if (IS_ERR(status))
    {
        comp_loader_deinit(&loader);
        return status;
    }

    char path[MAX_PATH];
    if (launchPathLen >= MAX_PATH)
    {
        comp_loader_deinit(&loader);
        return ERR(LIBSTD, TOOBIG);
    }
    memcpy(path, launchPath, launchPathLen);
    path[launchPathLen] = '\0';
    launchPath = path;
    launchPathLen++;

    status = comp_cull_orphans(&loader, main);
    if (IS_ERR(status))
    {
        comp_loader_deinit(&loader);
        return status;
    }

    fd_t root;
    status = iowalk(FDCWD, FDROOT, "/sys/fs/tmpfs/clone:rwx", &root);
    if (IS_ERR(status))
    {
        comp_loader_deinit(&loader);
        return status;
    }

    status = comp_load_capabilities(&loader, root);
    if (IS_ERR(status))
    {
        iodrop(root);
        comp_loader_deinit(&loader);
        return status;
    }

    status = comp_load_union(&loader, root);
    if (IS_ERR(status))
    {
        iodrop(root);
        comp_loader_deinit(&loader);
        return status;
    }

    proc_fd_t fds[5] = {
        {.parent = root, .child = FDROOT},
        {.parent = root, .child = FDCWD},
    };
    if (opts != NULL)
    {
        fds[2].parent = opts->stdin;
        fds[2].child = FDIN;
        fds[3].parent = opts->stdout;
        fds[3].child = FDOUT;
        fds[4].parent = opts->stderr;
        fds[4].child = FDERR;
    }
    status = proc_create(root, root, (proc_args_t){.buf = launchPath, .len = launchPathLen}, fds, opts != NULL ? 5 : 2,
        PRIO_DEFAULT, PROC_DEFAULT, NULL);
    if (IS_ERR(status))
    {
        printf("proc_create failed: %Y\n", status);
        iodrop(root);
        comp_loader_deinit(&loader);
        return status;
    }

    iodrop(root);
    comp_loader_deinit(&loader);
    return OK;
}
