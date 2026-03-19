#include <ctype.h>
#include <sys/scon.h>
#include <sys/comp.h>

typedef enum
{
    COMP_OP_EQUAL,
    COMP_OP_NOT_EQUAL,
    COMP_OP_GREATER,
    COMP_OP_GREATER_EQUAL,
    COMP_OP_LESS,
    COMP_OP_LESS_EQUAL,
} comp_version_op_t;

typedef struct
{
    uint32_t major;
    uint32_t minor;
    uint32_t patch;
} comp_version_t;

static status_t comp_version_parse(const char* version, comp_version_t* out)
{
    if (version == NULL || out == NULL)
    {
        return ERR(LIBSTD, INVAL);
    }

    const char* p = version;
    const char* end = version + strlen(version);

    out->major = 0;
    out->minor = 0;
    out->patch = 0;

    char* endptr;
    out->major = strtoul(p, &endptr, 10);
    if (p == endptr || *endptr != '.')
    {
        return ERR(LIBSTD, INVAL);
    }

    p = endptr + 1;
    out->minor = strtoul(p, &endptr, 10);
    if (p == endptr || *endptr != '.')
    {
        return ERR(LIBSTD, INVAL);
    }

    p = endptr + 1;
    out->patch = strtoul(p, &endptr, 10);
    if (p == endptr || (*endptr != '\0' && !isspace(*endptr)))
    {
        return ERR(LIBSTD, INVAL);
    }

    return OK;
}

static bool comp_version_compare(comp_version_t* current, comp_version_t next, comp_version_op_t op,
    comp_version_t target)
{
    switch (op)
    {
    case COMP_OP_EQUAL:
    {
        if (next.major == target.major && next.minor == target.minor && next.patch == target.patch)
        {
            return true;
        }
    }
    break;
    case COMP_OP_NOT_EQUAL:
    {
        return !comp_version_compare(current, next, COMP_OP_EQUAL, target);
    }
    break;
    case COMP_OP_GREATER:
    {
        if ((next.major > target.major || next.minor > target.minor || next.patch > target.patch) &&
            (current == NULL || next.major > current->major || next.minor > current->minor ||
                next.patch > current->patch))
        {
            return true;
        }
    }
    break;
    case COMP_OP_GREATER_EQUAL:
    {
        return comp_version_compare(current, next, COMP_OP_GREATER, target) ||
            comp_version_compare(current, next, COMP_OP_EQUAL, target);
    }
    break;
    case COMP_OP_LESS:
    {
        return !comp_version_compare(current, next, COMP_OP_GREATER_EQUAL, target);
    }
    break;
    case COMP_OP_LESS_EQUAL:
    {
        return !comp_version_compare(current, next, COMP_OP_GREATER, target);
    }
    break;
    }

    return false;
}

static status_t comp_version_select(const char* versions, size_t versionsLength, comp_version_op_t op,
    comp_version_t target, comp_version_t* out)
{
    const char* p = versions;
    comp_version_t current = {0, 0, 0};
    bool first = true;
    while (p < versions + versionsLength)
    {
        size_t len = strlen(p);

        comp_version_t next;
        status_t status = comp_version_parse(p, &next);
        if (IS_ERR(status))
        {
            return status;
        }

        if (comp_version_compare(!first ? &current : NULL, next, op, target))
        {
            current = next;
            first = false;
        }

        p += len + 1;
    }

    *out = current;
    return OK;
}

static status_t comp_version_op_parse(const char* op, size_t len, comp_version_op_t* out)
{
    if (op == NULL || out == NULL)
    {
        return ERR(LIBSTD, INVAL);
    }

    if (len != 1 && len != 2)
    {
        return ERR(LIBSTD, INVAL);
    }

    if (len == 1)
    {
        switch (op[0])
        {
        case '=':
            *out = COMP_OP_EQUAL;
            return OK;
        case '>':
            *out = COMP_OP_GREATER;
            return OK;
        case '<':
            *out = COMP_OP_LESS;
            return OK;
        default:
            return ERR(LIBSTD, INVAL);
        }
    }

    if (op[1] != '=')
    {
        return ERR(LIBSTD, INVAL);
    }

    switch (op[0])
    {
    case '!':
        *out = COMP_OP_NOT_EQUAL;
        return OK;
    case '>':
        *out = COMP_OP_GREATER_EQUAL;
        return OK;
    case '<':
        *out = COMP_OP_LESS_EQUAL;
        return OK;
    default:
        return ERR(LIBSTD, INVAL);
    }
}

status_t comp_launch(const char* name, const comp_launch_opts_t* opts)
{
    if (name == NULL)
    {
        return ERR(LIBSTD, INVAL);
    }

    comp_version_op_t op = COMP_OP_GREATER_EQUAL;
    comp_version_t target = {0, 0, 0};
    if (opts != NULL && opts->version != NULL)
    {
        const char* p = opts->version;
        size_t len = 0;
        while (*p != ' ' && *p != '\0')
        {
            len++;
            p++;
        }

        status_t status = comp_version_op_parse(p, len, &op);
        if (IS_ERR(status))
        {
            return status;
        }

        while (*p == ' ')
        {
            p++;
        }

        if (*p == '\0')
        {
            return ERR(LIBSTD, INVAL);
        }

        status = comp_version_parse(p, &target);
        if (IS_ERR(status))
        {
            return status;
        }
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

    if (versionsLength == 0)
    {
        free(versions);
        return ERR(LIBSTD, NOENT);
    }

    comp_version_t selected;
    status = comp_version_select(versions, versionsLength, op, target, &selected);
    free(versions);
    if (IS_ERR(status))
    {
        return status;
    }

    if (selected.major == 0 && selected.minor == 0 && selected.patch == 0)
    {
        return ERR(LIBSTD, INVAL);
    }

    size_t compLen = strlen(compPath);
    compPath[compLen++] = '/';
    snprintf(compPath + compLen, MAX_PATH - compLen, "%u.%u.%u", selected.major, selected.minor, selected.patch);

    char* manifest;
    size_t manifestLength;
    status = ioloadp(FDCWD, FDROOT, IOFMT("%s/manifest.scon", compPath), &manifest, &manifestLength);
    if (IS_ERR(status))
    {
        return status;
    }

    scon_t scon;
    status = scon_init(&scon, manifest, manifestLength);
    if (IS_ERR(status))
    {
        return status;
    }

    scon_ref_t root = scon_root(&scon);
    scon_ref_t compList = scon_find(root, "component");
    if (!scon_is_list(compList))
    {
        return ERR(LIBSTD, INVAL);
    }

    scon_ref_t launchList = scon_find(compList, "launch");
    if (!scon_is_list(launchList))
    {
        return ERR(LIBSTD, INVAL);
    }



    return OK;
}