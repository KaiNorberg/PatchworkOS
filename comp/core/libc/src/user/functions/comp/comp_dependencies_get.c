#include <libc/comp.h>
#include <stdlib.h>
#include <string.h>
#include <user/common/comp.h>

status_t comp_dependencies_get(const char* name, const char* version, comp_dependency_t** outDeps, size_t* outCount,
    char* error, size_t errorLength)
{
    if (name == NULL || version == NULL || outDeps == NULL || outCount == NULL)
    {
        return ERR(LIBSTD, INVAL);
    }

    _comp_loader_t loader;
    _comp_loader_init(&loader, error, errorLength);

    comp_version_t target;
    status_t status = _comp_version_parse(version, strlen(version), &target);
    if (IS_ERR(status))
    {
        _comp_error(&loader, "failed to parse component version '%s'", version);
        _comp_loader_deinit(&loader);
        return status;
    }

    _comp_req_t* main = NULL;
    status = _comp_add(&loader, name, strlen(name), version, strlen(version), &main);
    if (IS_ERR(status))
    {
        _comp_loader_deinit(&loader);
        return status;
    }

    status = _comp_load_dependencies(&loader);
    if (IS_ERR(status))
    {
        _comp_loader_deinit(&loader);
        return status;
    }

    status = _comp_cull_orphans(&loader, main);
    if (IS_ERR(status))
    {
        _comp_loader_deinit(&loader);
        return status;
    }

    size_t count = 0;
    _comp_req_t* req;
    LIST_FOR_EACH(req, &loader.reqs, entry)
    {
        count++;
    }

    comp_dependency_t* deps = NULL;
    if (count > 0)
    {
        deps = malloc(count * sizeof(comp_dependency_t));
        if (deps == NULL)
        {
            _comp_error(&loader, "out of memory allocating dependency array");
            _comp_loader_deinit(&loader);
            return ERR(LIBSTD, NOMEM);
        }

        size_t i = 0;
        LIST_FOR_EACH(req, &loader.reqs, entry)
        {
            memcpy_s(deps[i].name, ARRAY_SIZE(deps[i].name), req->name, ARRAY_SIZE(req->name));
            deps[i].version = req->version;
            scon_transfer(&deps[i].scon, &req->scon);
            deps[i].manifest = req->manifest;
            deps[i].manifestLength = req->manifestLength;
            req->manifest = NULL;
            req->manifestLength = 0;
            i++;
        }
    }

    _comp_loader_deinit(&loader);

    *outDeps = deps;
    *outCount = count;
    return OK;
}
