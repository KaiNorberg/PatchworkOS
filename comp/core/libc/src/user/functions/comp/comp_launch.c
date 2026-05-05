#include <libc/comp.h>
#include <libc/fs.h>
#include <libc/proc.h>
#include <libc/scon.h>
#include <stdio.h>
#include <string.h>
#include <user/common/comp.h>

status_t comp_launch(const char* name, const char* version, const comp_options_t* opts, char* error, size_t errorLength)
{
    UNUSED(opts);

    if (name == NULL || version == NULL)
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

    scon_item_t* compList = scon_find(scon_root(main->scon), "component");
    if (!scon_is_list(compList))
    {
        _comp_error(&loader, "manifest for component %s does not contain a component expression", name);
        _comp_loader_deinit(&loader);
        return ERR(LIBSTD, INVALSCON);
    }

    scon_item_t* launchList = scon_find(compList, "launch");
    if (!scon_is_list(launchList))
    {
        _comp_error(&loader, "manifest for component %s does not specify a launch executable", name);
        _comp_loader_deinit(&loader);
        return ERR(LIBSTD, INVALSCON);
    }

    scon_item_t* launch = scon_get(launchList, 1);
    if (!scon_is_atom(launch))
    {
        _comp_error(&loader, "manifest for component %s specifies an invalid launch executable", name);
        _comp_loader_deinit(&loader);
        return ERR(LIBSTD, INVALSCON);
    }

    const char* launchPath;
    size_t launchPathLen;
    status = scon_atom_get(launch, &launchPath, &launchPathLen);
    if (IS_ERR(status))
    {
        _comp_error(&loader, "failed to read launch executable string for component %s", name);
        _comp_loader_deinit(&loader);
        return status;
    }

    char path[MAX_PATH];
    if (launchPathLen >= MAX_PATH)
    {
        _comp_error(&loader, "launch executable path for component %s is too long", name);
        _comp_loader_deinit(&loader);
        return ERR(LIBSTD, TOOBIG);
    }
    memcpy(path, launchPath, launchPathLen);
    path[launchPathLen] = '\0';
    launchPath = path;
    launchPathLen++;

    status = _comp_cull_orphans(&loader, main);
    if (IS_ERR(status))
    {
        _comp_loader_deinit(&loader);
        return status;
    }

    _comp_req_t* req;
    LIST_FOR_EACH(req, &loader.reqs, entry)
    {
        scon_item_t* ref = scon_find(scon_root(req->scon), "component");
        if (!scon_is_list(ref))
        {
            continue;
        }

        scon_item_t* moduleRef = scon_find(ref, "module");
        if (moduleRef != NULL)
        {
            _comp_error(&loader, "dependency %s specifies a kernel module, which cannot be launched", req->name);
            _comp_loader_deinit(&loader);
            return ERR(LIBSTD, INVAL);
        }
    }

    fd_t root;
    status = iowalk(FDCWD, FDROOT, "/sys/fs/tmpfs/clone:rwx", &root);
    if (IS_ERR(status))
    {
        _comp_error(&loader, "failed to create component root tmpfs");
        _comp_loader_deinit(&loader);
        return status;
    }

    status = _comp_load_capabilities(&loader, root);
    if (IS_ERR(status))
    {
        iodrop(root);
        _comp_loader_deinit(&loader);
        return status;
    }

    status = _comp_load_union(&loader, root);
    if (IS_ERR(status))
    {
        iodrop(root);
        _comp_loader_deinit(&loader);
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
    status = proc_create(root, root, (proc_args_t){.buf = launchPath, .len = launchPathLen}, PROC_ENVP_INHERIT, fds, opts != NULL ? 5 : 2,
        PRIO_DEFAULT, PROC_DEFAULT, NULL);
    if (IS_ERR(status))
    {
        _comp_error(&loader, "failed to spawn process for component %s", name);
        iodrop(root);
        _comp_loader_deinit(&loader);
        return status;
    }

    iodrop(root);
    _comp_loader_deinit(&loader);
    return OK;
}
