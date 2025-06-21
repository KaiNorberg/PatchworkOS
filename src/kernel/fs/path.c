#include "path.h"

#include "defs.h"
#include "sched/thread.h"
#include "vfs.h"

#include <string.h>

static uint64_t flag_length(const char* flag)
{
    for (uint64_t flagLength = 0; flagLength < MAX_NAME; flagLength++)
    {
        if (PATH_END_OF_FLAG(flag[flagLength]))
        {
            return flagLength;
        }

        // Dont need to check if chars are valid as they either way wont match.
    }

    return ERR;
}

static bool flag_is_equal(const char* expected, const char* flag, uint64_t flagLength)
{
    if (strlen(expected) != flagLength)
    {
        return false;
    }

    for (uint64_t i = 0; i < flagLength; i++)
    {
        if (expected[i] != flag[i])
        {
            return false;
        }
    }

    return true;
}

static uint64_t name_length(const char* name)
{
    for (uint64_t nameLength = 0; nameLength < MAX_NAME; nameLength++)
    {
        if (PATH_END_OF_NAME(name[nameLength]))
        {
            return nameLength;
        }

        if (!PATH_VALID_CHAR(name[nameLength]))
        {
            return ERR;
        }
    }

    return ERR;
}

static const char* path_parse_flags(path_t* path, const char* src)
{
    const char* flag = src;
    while (1)
    {
        uint64_t flagLength = flag_length(flag);
        if (flagLength == ERR)
        {
            return ERRPTR(EBADFLAG);
        }

        if (flag_is_equal("nonblock", flag, flagLength))
        {
            path->flags |= PATH_NONBLOCK;
        }
        else if (flag_is_equal("append", flag, flagLength))
        {
            path->flags |= PATH_APPEND;
        }
        else if (flag_is_equal("create", flag, flagLength))
        {
            path->flags |= PATH_CREATE;
        }
        else if (flag_is_equal("excl", flag, flagLength))
        {
            path->flags |= PATH_EXCLUSIVE;
        }
        else if (flag_is_equal("trunc", flag, flagLength))
        {
            path->flags |= PATH_TRUNCATE;
        }
        else if (flag_is_equal("dir", flag, flagLength))
        {
            path->flags |= PATH_DIRECTORY;
        }
        else
        {
            return ERRPTR(EBADFLAG);
        }

        flag += flagLength;
        if (flag[0] == '\0')
        {
            return flag;
        }
        else if (flag[0] == PATH_FLAG_SEPARATOR)
        {
            flag += 1;
        }
        else
        {
            return ERRPTR(EBADPATH);
        }
    }
}

static const char* path_parse_names(path_t* path, const char* src)
{
    const char* name = src;
    while (1)
    {
        uint64_t nameLength = name_length(name);
        if (nameLength == ERR)
        {
            return ERRPTR(EBADPATH);
        }

        if (PATH_NAME_IS_DOT(name))
        {
            // Do nothing
        }
        else if (PATH_NAME_IS_DOT_DOT(name))
        {
            if (path->bufferLength == 0)
            {
                return ERRPTR(EINVAL);
            }

            do
            {
                path->bufferLength--;
            } while (path->bufferLength != 0 && path->buffer[path->bufferLength - 1] != '\0');
        }
        else if (nameLength != 0)
        {
            if (path->bufferLength + nameLength >= MAX_PATH)
            {
                return ERRPTR(ENAMETOOLONG);
            }

            memcpy(&path->buffer[path->bufferLength], name, nameLength);
            path->bufferLength += nameLength;

            path->buffer[path->bufferLength] = '\0';
            path->bufferLength++;
        }

        name += nameLength;
        if (name[0] == '\0' || name[0] == PATH_FLAGS_SEPARATOR)
        {
            if (path->bufferLength == MAX_PATH)
            {
                return ERRPTR(ENAMETOOLONG);
            }

            path->buffer[path->bufferLength] = '\3';
            return name[0] == '\0' ? name : name + 1;
        }
        else if (name[0] == PATH_NAME_SEPARATOR)
        {
            name += 1;
        }
        else
        {
            return ERRPTR(EBADPATH);
        }
    }
}

static const char* path_determine_type(path_t* path, const char* string, path_t* cwd)
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

        return string + 1;
    }

    bool absolute = false;
    uint64_t i = 0;
    for (; !PATH_END_OF_NAME(string[i]); i++)
    {
        if (string[i] == PATH_LABEL_SEPARATOR)
        {
            if (!PATH_END_OF_NAME(string[i + 1]))
            {
                return ERRPTR(EBADPATH);
            }

            absolute = true;
            break;
        }
        else if (!PATH_VALID_CHAR(string[i]))
        {
            return ERRPTR(EBADPATH);
        }
    }

    if (absolute) // Absolute path
    {
        uint64_t volumeLength = i;
        memcpy(path->volume, string, volumeLength);
        path->volume[volumeLength] = '\0';

        return string[volumeLength + 1] == PATH_NAME_SEPARATOR ? string + volumeLength + 2 : string + volumeLength + 1;
    }
    else // Relative path
    {
        if (cwd == NULL)
        {
            return ERRPTR(EINVAL);
        }

        strcpy(path->volume, cwd->volume);

        memcpy(path->buffer, cwd->buffer, cwd->bufferLength + 1);
        path->bufferLength = cwd->bufferLength;

        return string;
    }
}

uint64_t path_init(path_t* path, const char* string, path_t* cwd)
{
    path->volume[0] = '\0';
    path->buffer[0] = '\3';
    path->bufferLength = 0;
    path->flags = 0;
    path->isInvalid = false;

    if (string == NULL)
    {
        path->isInvalid = true;
        return ERR;
    }

    string = path_determine_type(path, string, cwd);
    if (string == NULL)
    {
        path->isInvalid = true;
        return ERR;
    }

    if (string[0] == '\0')
    {
        return 0;
    }

    string = path_parse_names(path, string);
    if (string == NULL)
    {
        path->isInvalid = true;
        return ERR;
    }

    if (string[0] == '\0')
    {
        return 0;
    }

    string = path_parse_flags(path, string);
    if (string == NULL)
    {
        path->isInvalid = true;
        return ERR;
    }

    return 0;
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

node_t* path_traverse_node_parent(const path_t* path, node_t* node)
{
    if (path->bufferLength == 0 || path->buffer[0] == '\3')
    {
        return NULL;
    }

    node_t* previous = node;
    const char* name;
    PATH_FOR_EACH(name, path)
    {
        const char* next = name + strlen(name) + 1;
        if (next[0] == '\3')
        {
            return previous;
        }

        node_t* child = node_find(previous, name);
        if (child == NULL)
        {
            return NULL;
        }

        previous = child;
    }

    return previous;
}

bool path_is_name_valid(const char* name)
{
    const char* chr = name;
    while (*chr != '\0')
    {
        if (!PATH_VALID_CHAR(*chr))
        {
            return false;
        }
        chr++;
    }
    return true;
}

char* path_last_name(const path_t* path)
{
    if (path->bufferLength == 0 || path->buffer[0] == '\3')
    {
        return NULL;
    }

    char* last = NULL;
    const char* name;
    PATH_FOR_EACH(name, path)
    {
        last = (char*)name;
    }

    return last;
}

#ifdef TESTING
#include "utils/testing.h"

static uint64_t path_test(const char* cwdStr, const char* testPath, const char* expectedResult)
{
    path_t cwd;
    path_t path;
    char parsedPath[MAX_PATH];
    char parsedCwd[MAX_PATH];

    if (cwdStr != NULL)
    {
        path_init(&cwd, cwdStr, NULL);
    }

    uint64_t result = path_init(&path, testPath, cwdStr != NULL ? &cwd : NULL);

    if (result == ERR)
    {
        if (expectedResult != NULL)
        {
            log_print(LOG_INFO, "failed: unexpectedly returned error, cwd=\"%s\" path=\"%s\"\n",
                cwdStr != NULL ? cwdStr : "NULL", testPath);
            return ERR;
        }
        return 0;
    }

    path_to_string(&path, parsedPath);
    path_to_string(&cwd, parsedCwd);

    if (strcmp(parsedPath, expectedResult) != 0)
    {
        log_print(LOG_INFO, "failed: cwd=\"%s\" parsed_cwd=\"%s\" path=\"%s\" expected=\"%s\" result=\"%s\"\n",
            cwdStr != NULL ? cwdStr : "NULL", parsedCwd, testPath, expectedResult, parsedPath);
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
    result |=
        path_test("data:/users/admin", "documents///photos//vacation/", "data:/users/admin/documents/photos/vacation");
    result |=
        path_test("data:/users/admin", "./downloads/../documents/./reports/../../photos", "data:/users/admin/photos");
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
    result |= path_test("project:/src",
        "lib/core/utils/string/parser/../../network/http/client/api/v1/../../../../../../tests",
        "project:/src/lib/core/tests");
    result |= path_test("docs:/", "research/paper (draft 2).pdf", "docs:/research/paper (draft 2).pdf");
    result |= path_test("media:/music", "Albums/Rock & Roll/Bands", "media:/music/Albums/Rock & Roll/Bands");
    result |= path_test("backup:/2023", "files_v1.2-beta+build.3", "backup:/2023/files_v1.2-beta+build.3");
    result |= path_test("backup:/2023", "sys:/net/local/new?nonblock", "sys:/net/local/new");

    return result;
}

#endif
