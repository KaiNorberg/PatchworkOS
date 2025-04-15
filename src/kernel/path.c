#include "path.h"

#include "defs.h"
#include "sched.h"
#include "vfs.h"

#include <stdio.h>
#include <string.h>

static bool name_compare(const char* a, const char* b)
{
    for (uint64_t i = 0; i < MAX_PATH; i++)
    {
        if (PATH_END_OF_NAME(a[i]))
        {
            return a[i] == b[i];
        }
        if (a[i] != b[i])
        {
            return false;
        }
    }

    return false;
}

static const char* name_next(const char* path)
{
    const char* base = strchr(path, PATH_NAME_SEPARATOR);
    return base != NULL ? base + 1 : NULL;
}

static uint64_t path_make_canonical(char* dest, char* out, const char* src)
{
    const char* name = src;
    while (1)
    {
        if (name_compare(name, "."))
        {
            // Do nothing
        }
        else if (name_compare(name, ".."))
        {
            if (out == dest)
            {
                return ERROR(EPATH);
            }

            if (*out == '\3')
            {
                out -= 2;
            }
            else
            {
                out -= 1;
            }
            while (*(--out) != '\0' && out != dest)
                ;
        }
        else
        {
            const char* ptr = name;
            while (!PATH_END_OF_NAME(*ptr))
            {
                if (!PATH_VALID_CHAR(*ptr) || (uint64_t)(out - dest) >= MAX_PATH - 2)
                {
                    return ERROR(EPATH);
                }

                *out++ = *ptr++;
            }

            if (ptr != name)
            {
                *out++ = '\0';
            }
        }

        name = name_next(name);
        if (name == NULL || name[0] == '\0')
        {
            if (*out == '\0')
            {
                out++;
            }
            *out = '\3';
            return 0;
        }
    }
}

uint64_t path_init(path_t* path, const char* string, path_t* cwd)
{
    if (string[0] == PATH_NAME_SEPARATOR) // Root path
    {
        if (cwd != NULL)
        {
            strcpy(path->volume, cwd->volume);
        }
        else
        {
            path->volume[0] = '\0';
        }

        return path_make_canonical(path->buffer, path->buffer, string);
    }

    bool absolute = false;
    uint64_t i = 0;
    for (; !PATH_END_OF_NAME(string[i]); i++)
    {
        if (string[i] == PATH_LABEL_SEPARATOR)
        {
            if (!PATH_END_OF_NAME(string[i + 1]))
            {
                return ERROR(EPATH);
            }

            absolute = true;
            break;
        }
        else if (!PATH_VALID_CHAR(string[i]))
        {
            return ERROR(EPATH);
        }
    }

    if (absolute) // Absolute path
    {
        uint64_t volumeLength = i;
        memcpy(path->volume, string, volumeLength);
        path->volume[volumeLength] = '\0';

        return path_make_canonical(path->buffer, path->buffer, string + volumeLength + 2);
    }
    else // Relative path
    {
        if (cwd == NULL)
        {
            return ERROR(EINVAL);
        }

        strcpy(path->volume, cwd->volume);

        uint64_t cwdLength = ((uint64_t)memchr(cwd->buffer, '\3', MAX_PATH) - (uint64_t)cwd->buffer);
        memcpy(path->buffer, cwd->buffer, cwdLength + 1);

        return path_make_canonical(path->buffer, path->buffer + cwdLength, string);
    }
}

void path_to_string(const path_t* path, char* dest)
{
    dest[0] = '\0';
    if (path->volume[0] != '\0')
    {
        strcpy(dest, path->volume);
        strcat(dest, ":");
    }

    uint64_t nameAmount = 0;

    const char* name;
    PATH_FOR_EACH(name, path)
    {
        strcat(dest, "/");
        strcat(dest, name);
        nameAmount++;
    }

    if (nameAmount == 0)
    {
        strcat(dest, "/");
    }
}

node_t* path_traverse_node(const path_t* path, node_t* node)
{
    const char* name;
    PATH_FOR_EACH(name, path)
    {
        node_t* child = node_find(node, name);
        if (child == NULL)
        {
            return NULL;
        }

        node = child;
    }

    return node;
}
