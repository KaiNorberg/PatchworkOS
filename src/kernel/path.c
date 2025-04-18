#include "path.h"

#include "defs.h"
#include "sched.h"
#include "vfs.h"

#include <stdio.h>
#include <string.h>

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
        if (PATH_NAME_IS_DOT(name))
        {
            // Do nothing
        }
        else if (PATH_NAME_IS_DOT_DOT(name))
        {
            if (out == dest)
            {
                return ERROR(EPATH);
            }

            do
            {
                out--;
            } while (out != dest && *(out - 1) != '\0');
        }
        else
        {
            uint64_t i = 0;
            for (; i < MAX_NAME; i++)
            {
                if (PATH_END_OF_NAME(name[i]))
                {
                    break;
                }

                *out = name[i];
                out++;
            }

            if (i != 0)
            {
                *out = '\0';
                out++;
            }
        }

        name = name_next(name);
        if (name == NULL || name[0] == '\0')
        {
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

#ifdef TESTING
#include "testing.h"

static uint64_t path_test(const char* cwdStr, const char* testPath, const char* expectedResult)
{
    path_t cwd;
    path_t path;
    char buffer[MAX_PATH];

    if (cwdStr != NULL)
    {
        path_init(&cwd, cwdStr, NULL);
    }

    uint64_t result = path_init(&path, testPath, cwdStr != NULL ? &cwd : NULL);

    if (result == ERR)
    {
        if (expectedResult != NULL)
        {
            printf("failed: unexpectedly returned error, cwd=\"%s\" path=\"%s\"", cwdStr != NULL ? cwdStr : "NULL", testPath);
            return ERR;
        }
        return 0;
    }

    path_to_string(&path, buffer);

    if (strcmp(buffer, expectedResult) != 0)
    {
        printf("failed: cwd=\"%s\" path=\"%s\" expected=\"%s\" result=\"%s\"", cwdStr != NULL ? cwdStr : "NULL", testPath,
            expectedResult, buffer);
        return ERR;
    }

    return 0;
}

TESTING_REGISTER_TEST(path_all_tests)
{
    uint64_t result = 0;
    result |= path_test("sys:/proc", "sys:/kbd/ps2", "sys:/kbd/ps2");
    result |= path_test("sys:/proc", ".", "sys:/proc");
    result |= path_test("sys:/proc", "..", "sys:/");
    result |= path_test("sys:/proc", "../dev/./null", "sys:/dev/null");
    result |= path_test("sys:/", "home/user", "sys:/home/user");
    result |= path_test("sys:/usr/local/bin", "../lib", "sys:/usr/local/lib");
    result |= path_test("sys:/usr/local/bin", "../../../", "sys:/");
    result |= path_test("sys:/usr/local/bin", "usr:/bin", "usr:/bin");
    result |= path_test("usr:/lib", "include", "usr:/lib/include");
    result |= path_test("usr:/lib", "sys:/proc", "sys:/proc");
    result |= path_test("usr:/lib", "", "usr:/lib");
    result |= path_test("usr:/lib", "/", "usr:/");
    result |= path_test("data:/users/admin", "documents///photos//vacation/", "data:/users/admin/documents/photos/vacation");
    result |= path_test("data:/users/admin", "./downloads/../documents/./reports/../../photos", "data:/users/admin/photos");
    result |= path_test("data:/users/admin", "notes/report (2023).txt", "data:/users/admin/notes/report (2023).txt");
    result |= path_test("data:/users/admin", "bad|file?name", NULL);
    result |= path_test(NULL, "relative/path", NULL);
    result |= path_test("data:/users/admin", "bad:volume/path", NULL);
    result |= path_test("app:/games", "rpg/saves/.", "app:/games/rpg/saves");
    result |= path_test("app:/games", "rpg/saves/..", "app:/games/rpg");
    result |= path_test("app:/games", "rpg/../../games/shooter", "app:/games/shooter");
    result |= path_test("temp:/downloads", "log:/system/errors", "log:/system/errors");
    result |= path_test("temp:/downloads", "temp:/uploads", "temp:/uploads");
    result |= path_test("root:/", "/", "root:/");
    result |= path_test("root:/", "/bin", "root:/bin");
    result |= path_test("dev:/tools", "//multiple//slashes///", "dev:/multiple/slashes");
    result |= path_test("sys:/usr/bin", "/", "sys:/");
    result |= path_test("etc:/config", "home/user/.config/app/./../..", "etc:/config/home/user");
    result |= path_test("project:/src", "lib/core/utils/string/parser/../../network/http/client/api/v1/../../../../../../tests",
        "project:/src/lib/core/tests");
    result |= path_test("docs:/", "research/paper (draft 2).pdf", "docs:/research/paper (draft 2).pdf");
    result |= path_test("media:/music", "Albums/Rock & Roll/Bands", "media:/music/Albums/Rock & Roll/Bands");
    result |= path_test("backup:/2023", "files_v1.2-beta+build.3", "backup:/2023/files_v1.2-beta+build.3");

    return result;
}

#endif
