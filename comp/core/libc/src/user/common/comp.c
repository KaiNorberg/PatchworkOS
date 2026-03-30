#include "comp.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COMP_CAP_MAX 64

typedef struct
{
    char* path;
    size_t length;
} _comp_cap_t;

typedef struct
{
    _comp_cap_t caps[COMP_CAP_MAX];
    size_t count;
} _comp_cap_set_t;

typedef enum
{
    COMP_DIR_BIN,
    COMP_DIR_LIB,
    COMP_DIR_INCLUDE,
    COMP_DIR_DATA,
    COMP_DIR_CFG,
    COMP_DIR_MAX,
} _comp_dirs_t;

typedef struct
{
    fd_t sources[COMP_CAP_MAX];
    size_t count;
} _comp_union_t;

static bool _comp_req_compare(map_entry_t* entry, const void* key)
{
    _comp_req_t* req = CONTAINER_OF(entry, _comp_req_t, mapEntry);
    return strcmp(req->name, (const char*)key) == 0;
}

void _comp_error(_comp_loader_t* loader, const char* format, ...)
{
    if (loader != NULL && loader->error != NULL && loader->errorLength > 0)
    {
        va_list args;
        va_start(args, format);
        vsnprintf(loader->error, loader->errorLength, format, args);
        va_end(args);
    }
}

void _comp_loader_init(_comp_loader_t* loader, char* error, size_t errorLength)
{
    list_init(&loader->reqs);
    MAP_DEFINE_INIT(loader->map, _comp_req_compare);
    loader->error = error;
    loader->errorLength = errorLength;
    if (loader->error != NULL && loader->errorLength > 0)
    {
        loader->error[0] = '\0';
    }
}

void _comp_loader_deinit(_comp_loader_t* loader)
{
    _comp_req_t* req;
    _comp_req_t* temp;
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

static int64_t _comp_version_compare(comp_version_t a, comp_version_t b)
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

status_t _comp_version_parse(const char* version, size_t versionLen, comp_version_t* out)
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

status_t _comp_add(_comp_loader_t* loader, const char* name, size_t nameLen, const char* version, size_t versionLen,
    _comp_req_t** out)
{
    if (loader == NULL || name == NULL || nameLen >= MAX_NAME || version == NULL || out == NULL)
    {
        return ERR(LIBSTD, INVAL);
    }

    _comp_req_t* req = malloc(sizeof(_comp_req_t));
    if (req == NULL)
    {
        _comp_error(loader, "out of memory allocating component request");
        return ERR(LIBSTD, NOMEM);
    }

    memcpy(req->name, name, nameLen);
    req->name[nameLen] = '\0';

    status_t status = _comp_version_parse(version, versionLen, &req->version);
    if (IS_ERR(status))
    {
        _comp_error(loader, "failed to parse version string for component %.*s", (int)nameLen, name);
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

static status_t _comp_find_lowest_available(_comp_loader_t* loader, const char* name, comp_version_t reqVersion,
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
        _comp_error(loader, "failed to load versions for component %s", name);
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
        status = _comp_version_parse(p, len, &version);
        if (IS_ERR(status))
        {
            p += len + 1;
            continue;
        }

        if (_comp_version_compare(version, reqVersion) >= 0)
        {
            if (_comp_version_compare(version, selected) < 0)
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
        _comp_error(loader, "no available version of component %s satisfies dependency %u.%u.%u", name,
            reqVersion.major, reqVersion.minor, reqVersion.patch);
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

static status_t _comp_check(_comp_loader_t* loader, _comp_req_t* req)
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
    status_t status = _comp_find_lowest_available(loader, req->name, req->version,
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
        _comp_error(loader, "failed to load manifest for component %s", req->name);
        return status;
    }

    status = scon_init(&req->scon, req->manifest, req->manifestLength);
    if (IS_ERR(status))
    {
        _comp_error(loader, "failed to parse manifest for component %s: %s", req->name, req->scon.error);
        return status;
    }

    scon_ref_t compList = scon_find(scon_root(&req->scon), "component");
    if (!scon_is_list(compList))
    {
        _comp_error(loader, "manifest for component %s does not contain a component expression", req->name);
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
        _comp_req_t* depReq = entry ? CONTAINER_OF(entry, _comp_req_t, mapEntry) : NULL;

        if (depReq == NULL)
        {
            status = _comp_add(loader, depName, nameLen, versionStr, versionLen, &depReq);
            if (IS_ERR(status))
            {
                return status;
            }
        }
        else
        {
            comp_version_t reqDepVersion;
            if (IS_ERR(_comp_version_parse(versionStr, versionLen, &reqDepVersion)))
            {
                continue;
            }

            if (_comp_version_compare(depReq->version, reqDepVersion) < 0)
            {
                depReq->version = reqDepVersion;
                depReq->processed = false;
            }
        }
    }

    req->processed = true;
    return OK;
}

status_t _comp_load_dependencies(_comp_loader_t* loader)
{
    if (loader == NULL)
    {
        return ERR(LIBSTD, INVAL);
    }

    bool resolving = true;
    while (resolving)
    {
        resolving = false;
        _comp_req_t* req;
        LIST_FOR_EACH(req, &loader->reqs, entry)
        {
            if (req->processed)
            {
                continue;
            }

            status_t status = _comp_check(loader, req);
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

static void _comp_mark_reachable(_comp_loader_t* loader, _comp_req_t* req, list_t* orderedList)
{
    if (req->processed)
    {
        return;
    }

    req->processed = true;

    if (req->manifest == NULL)
    {
        list_remove(&req->entry);
        list_push_back(orderedList, &req->entry);
        return;
    }

    scon_ref_t compList = scon_find(scon_root(&req->scon), "component");
    if (!scon_is_list(compList))
    {
        list_remove(&req->entry);
        list_push_back(orderedList, &req->entry);
        return;
    }

    scon_ref_t dependencies = scon_find(compList, "dependencies");
    if (!scon_is_list(dependencies))
    {
        list_remove(&req->entry);
        list_push_back(orderedList, &req->entry);
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
            _comp_req_t* depReq = CONTAINER_OF(entry, _comp_req_t, mapEntry);
            _comp_mark_reachable(loader, depReq, orderedList);
        }
    }

    list_remove(&req->entry);
    list_push_back(orderedList, &req->entry);
}

status_t _comp_cull_orphans(_comp_loader_t* loader, _comp_req_t* main)
{
    if (loader == NULL || main == NULL)
    {
        return ERR(LIBSTD, INVAL);
    }

    _comp_req_t* req;
    LIST_FOR_EACH(req, &loader->reqs, entry)
    {
        req->processed = false;
    }

    list_t orderedList;
    list_init(&orderedList);

    _comp_mark_reachable(loader, main, &orderedList);

    _comp_req_t* temp;
    LIST_FOR_EACH_SAFE(req, temp, &loader->reqs, entry)
    {
        map_remove(&loader->map, &req->mapEntry, hash_string(req->name));
        if (req->manifest != NULL)
        {
            scon_deinit(&req->scon);
            free(req->manifest);
        }
        list_remove(&req->entry);
        free(req);
    }

    LIST_FOR_EACH_SAFE(req, temp, &orderedList, entry)
    {
        list_remove(&req->entry);
        list_push_back(&loader->reqs, &req->entry);
    }

    return OK;
}

static void _comp_cap_set_deinit(_comp_cap_set_t* set)
{
    for (size_t i = 0; i < set->count; i++)
    {
        free(set->caps[i].path);
    }
    set->count = 0;
}

static status_t _comp_cap_set_add(_comp_cap_set_t* set, const char* path, size_t length)
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

static status_t _comp_load_capability(_comp_cap_set_t* caps, scon_ref_t cap)
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

    status_t status = _comp_cap_set_add(caps, capStr, capLen);
    if (IS_ERR(status))
    {
        return status;
    }

    return OK;
}

status_t _comp_load_capabilities(_comp_loader_t* loader, fd_t root)
{
    if (loader == NULL)
    {
        return ERR(LIBSTD, INVAL);
    }

    _comp_cap_set_t caps;
    caps.count = 0;

    _comp_req_t* req;
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
            status_t status = _comp_load_capability(&caps, cap);
            if (IS_ERR(status))
            {
                _comp_cap_set_deinit(&caps);
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
            _comp_error(loader, "failed to walk capability path '%s'", caps.caps[i].path);
            break;
        }

        file_type_t type = 0;
        status = ioattr(src, FILE_GET_TYPE, &type);
        if (IS_ERR(status))
        {
            _comp_error(loader, "failed to get type for capability '%s'", caps.caps[i].path);
            iodrop(src);
            break;
        }

        fd_t target;
        status =
            iowalk(root, root, IOFMT("%s:%s", caps.caps[i].path, type == FILE_TYPE_DIRECTORY ? "dp" : "cp"), &target);
        if (IS_ERR(status))
        {
            _comp_error(loader, "failed to bind capability '%s' to tmpfs", caps.caps[i].path);
            iodrop(src);
            break;
        }

        status = fdbind(root, target, src);
        iodrop(target);
        iodrop(src);

        if (IS_ERR(status))
        {
            _comp_error(loader, "failed to bind capability '%s'", caps.caps[i].path);
            break;
        }
    }

    _comp_cap_set_deinit(&caps);
    return status;
}

status_t _comp_load_union(_comp_loader_t* loader, fd_t root)
{
    if (loader == NULL)
    {
        return ERR(LIBSTD, INVAL);
    }

    _comp_union_t unions[COMP_DIR_MAX];
    for (uint32_t i = 0; i < COMP_DIR_MAX; i++)
    {
        unions[i].count = 0;
    }

    const char* dirNames[COMP_DIR_MAX] = {"bin", "lib", "include", "share", "cfg"};

    _comp_req_t* req;
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
            _comp_error(loader, "failed to create directory '%s' in tmpfs", dirNames[i]);
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
            _comp_error(loader, "failed to create concatfs for directory '%s'", dirNames[i]);
            iodrop(target);
            break;
        }

        status = fdbind(root, target, concatfs);
        iodrop(concatfs);
        iodrop(target);
        if (IS_ERR(status))
        {
            _comp_error(loader, "failed to bind concatfs to directory '%s'", dirNames[i]);
            break;
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
