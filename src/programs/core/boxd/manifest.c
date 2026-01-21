#include "manifest.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <sys/fs.h>
#include <sys/math.h>
#include <sys/proc.h>

static char* trim_whitespace(char* str)
{
    char* end;

    while (isspace((unsigned char)*str))
    {
        str++;
    }

    if (*str == '\0')
    {
        return str;
    }

    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end))
    {
        end--;
    }

    end[1] = '\0';

    return str;
}

uint64_t manifest_parse(manifest_t* manifest, const char* path)
{
    FILE* file = fopen(path, "r");
    if (file == NULL)
    {
        return ERR;
    }

    memset(manifest, 0, sizeof(manifest_t));

    char line[MAX_PATH] = {0};
    section_t* section = NULL;
    while (fgets(line, sizeof(line), file) != NULL)
    {
        char* p = trim_whitespace(line);

        if (p[0] == '#' || p[0] == ';' || p[0] == '\0')
        {
            continue;
        }

        if (p[0] == '[')
        {
            char* end = strchr(p, ']');
            if (end == NULL)
            {
                continue;
            }

            *end = '\0';
            char* name = p + 1;
            if (strcmp(name, "meta") == 0)
            {
                section = &manifest->sections[SECTION_META];
            }
            else if (strcmp(name, "exec") == 0)
            {
                section = &manifest->sections[SECTION_EXEC];
            }
            else if (strcmp(name, "env") == 0)
            {
                section = &manifest->sections[SECTION_ENV];
            }
            else if (strcmp(name, "sandbox") == 0)
            {
                section = &manifest->sections[SECTION_SANDBOX];
            }
            else if (strcmp(name, "namespace") == 0)
            {
                section = &manifest->sections[SECTION_NAMESPACE];
            }
            else
            {
                section = NULL;
            }

            continue;
        }

        if (section == NULL)
        {
            continue;
        }

        char* equal = strchr(p, '=');
        if (equal == NULL)
        {
            continue;
        }

        *equal = '\0';

        char* key = trim_whitespace(p);
        char* value = trim_whitespace(equal + 1);

        if (section->amount >= MANIFEST_SECTION_MAX)
        {
            continue;
        }

        strncpy(section->entries[section->amount].key, key, MANIFEST_STRING_MAX - 1);
        strncpy(section->entries[section->amount].value, value, MANIFEST_STRING_MAX - 1);
        section->amount++;
    }

    fclose(file);
    return 0;
}

void manifest_substitute(manifest_t* manifest, substitution_t* substitutions, uint64_t amount)
{
    for (uint64_t i = 0; i < SECTION_TYPE_MAX; i++)
    {
        section_t* section = &manifest->sections[i];

        for (uint64_t j = 0; j < section->amount; j++)
        {
            section_entry_t* entry = &section->entries[j];

            for (uint64_t k = 0; k < amount; k++)
            {
                substitution_t* sub = &substitutions[k];
                char search[MANIFEST_STRING_MAX];

                if (snprintf(search, sizeof(search), "$%s", sub->key) >= (int)sizeof(search))
                {
                    continue;
                }

                size_t searchLen = strlen(search);
                size_t replaceLen = strlen(sub->value);

                char* ptr = entry->value;

                while ((ptr = strstr(ptr, search)) != NULL)
                {
                    size_t currentLen = strlen(entry->value);

                    if (currentLen - searchLen + replaceLen >= MANIFEST_STRING_MAX)
                    {
                        break;
                    }

                    memmove(ptr + replaceLen, ptr + searchLen, strlen(ptr + searchLen) + 1);
                    memcpy(ptr, sub->value, replaceLen);

                    ptr += replaceLen;
                }
            }
        }
    }
}

char* manifest_get_value(section_t* section, const char* key)
{
    for (uint64_t i = 0; i < section->amount; i++)
    {
        if (strcmp(section->entries[i].key, key) == 0)
        {
            return section->entries[i].value;
        }
    }
    return NULL;
}

uint64_t manifest_get_integer(section_t* section, const char* key)
{
    char* value = manifest_get_value(section, key);
    if (value == NULL)
    {
        return ERR;
    }

    size_t len = strlen(value);
    if (len == 0)
    {
        return ERR;
    }

    for (size_t i = 0; i < len; i++)
    {
        if (!isdigit(value[i]))
        {
            return ERR;
        }
    }

    return atoll(value);
}