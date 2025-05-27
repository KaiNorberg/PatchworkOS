#include <libpatchwork/patchwork.h>

#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/io.h>
#include <sys/list.h>

// TODO: Implement recursive sections.

typedef struct
{
    list_entry_t entry;
    char key[MAX_PATH];
    char value[MAX_PATH];
} config_pair_t;

typedef struct
{
    list_entry_t entry;
    list_t children;
    list_t pairs;
    char name[MAX_PATH];
} config_section_t;

typedef struct config
{
    config_section_t* root;
} config_t;

typedef struct config_array
{
    char** strings;
    uint64_t length;
} config_array_t;

static config_pair_t* config_pair_new(const char* key, const char* value)
{
    config_pair_t* pair = malloc(sizeof(config_pair_t));
    if (pair == NULL)
    {
        return NULL;
    }
    list_entry_init(&pair->entry);

    strncpy(pair->key, key, MAX_PATH - 1);
    pair->key[MAX_PATH - 1] = '\0';
    strncpy(pair->value, value, MAX_PATH - 1);
    pair->value[MAX_PATH - 1] = '\0';

    return pair;
}

static config_section_t* config_section_new(void)
{
    config_section_t* section = malloc(sizeof(config_section_t));
    if (section == NULL)
    {
        return NULL;
    }
    list_entry_init(&section->entry);
    list_init(&section->children);
    list_init(&section->pairs);
    section->name[0] = '\0';
    return section;
}

static void config_section_free(config_section_t* section)
{
    if (section == NULL)
    {
        return;
    }

    config_section_t* child;
    config_section_t* temp1;
    LIST_FOR_EACH_SAFE(child, temp1, &section->children, entry)
    {
        config_section_free(child);
    }

    config_pair_t* pair;
    config_pair_t* temp2;
    LIST_FOR_EACH_SAFE(pair, temp2, &section->pairs, entry)
    {
        list_remove(&pair->entry);
        free(pair);
    }

    list_remove(&section->entry);
    free(section);
}

config_t* config_open(const char* prefix, const char* name)
{
    if (prefix == NULL || name == NULL)
    {
        return NULL;
    }

    if (strchr(prefix, '/') != NULL || strchr(name, '/') != NULL)
    {
        return NULL;
    }

    char path[MAX_PATH];
    int ret = snprintf(path, sizeof(path), "/cfg/%s-%s.cfg", prefix, name);
    if (ret < 0 || ret >= (int)sizeof(path))
    {
        return NULL;
    }

    FILE* file = fopen(path, "r");
    if (file == NULL)
    {
        return NULL;
    }

    config_t* cfg = malloc(sizeof(config_t));
    if (cfg == NULL)
    {
        fclose(file);
        return NULL;
    }

    cfg->root = config_section_new();
    if (cfg->root == NULL)
    {
        free(cfg);
        fclose(file);
        return NULL;
    }

    char buffer[MAX_PATH + 2];
    config_section_t* currentSection = cfg->root;

    while (fgets(buffer, sizeof(buffer), file) != NULL)
    {
        char* line = buffer;

        // Trim leading whitespace
        while (isspace((unsigned char)*line))
        {
            line++;
        }

        // Trim trailing whitespace
        char* end = line + strlen(line) - 1;
        while (end >= line && isspace((unsigned char)*end))
        {
            *end = '\0';
            end--;
        }

        uint64_t lineLen = strlen(line);
        if (lineLen == 0)
        {
            continue; // Skip empty lines
        }

        if (line[0] == ';' || line[0] == '#')
        {
            continue; // Skip comments
        }

        if (line[0] == '[' && line[lineLen - 1] == ']')
        {
            char section[MAX_PATH];
            uint64_t sectionLen = lineLen - 2;

            if (sectionLen >= MAX_PATH)
            {
                goto error;
            }

            memcpy(section, line + 1, sectionLen);
            section[sectionLen] = '\0';

            config_section_t* newSection = config_section_new();
            if (newSection == NULL)
            {
                goto error;
            }

            strncpy(newSection->name, section, MAX_PATH - 1);
            newSection->name[MAX_PATH - 1] = '\0';

            list_push(&cfg->root->children, &newSection->entry);
            currentSection = newSection;
            continue;
        }

        char* equals = strchr(line, '=');
        if (equals != NULL)
        {
            char* keyStart = line;
            char* keyEnd = equals - 1;

            while (keyEnd >= keyStart && isspace((unsigned char)*keyEnd))
            {
                keyEnd--;
            }

            uint64_t keyLen = (keyEnd - keyStart) + 1;
            if (keyLen == 0 || keyLen >= MAX_PATH)
            {
                goto error;
            }

            char key[MAX_PATH];
            memcpy(key, keyStart, keyLen);
            key[keyLen] = '\0';

            char* valueStart = equals + 1;

            while (isspace((unsigned char)*valueStart))
            {
                valueStart++;
            }

            uint64_t valueLen = strlen(valueStart);
            if (valueLen >= MAX_PATH)
            {
                goto error;
            }

            char value[MAX_PATH];
            strcpy(value, valueStart);

            config_pair_t* pair = config_pair_new(key, value);
            if (pair == NULL)
            {
                goto error;
            }

            list_push(&currentSection->pairs, &pair->entry);
            continue;
        }

        goto error;
    }

    fclose(file);
    return cfg;

error:
    if (cfg != NULL)
    {
        config_section_free(cfg->root);
        free(cfg);
    }
    fclose(file);
    return NULL;
}

void config_close(config_t* cfg)
{
    if (cfg == NULL)
    {
        return;
    }

    config_section_free(cfg->root);
    free(cfg);
}

uint64_t config_scanf(config_t* cfg, const char* section, const char* key, const char* format, ...)
{
    const char* str = config_string_get(cfg, section, key, NULL);
    if (str == NULL)
    {
        return ERR;
    }

    va_list arg;
    va_start(arg, format);
    int result = vsscanf(str, format, arg);
    va_end(arg);
    return result;
}

const char* config_string_get(config_t* cfg, const char* section, const char* key, const char* fallback)
{
    if (cfg == NULL || section == NULL || key == NULL)
    {
        return fallback;
    }

    config_section_t* targetSection;
    if (section == NULL || section[0] == '\0')
    {
        targetSection = cfg->root;
    }
    else
    {
        bool found = false;
        LIST_FOR_EACH(targetSection, &cfg->root->children, entry)
        {
            if (strcasecmp(targetSection->name, section) == 0)
            {
                found = true;
                break;
            }
        }

        if (!found)
        {
            return fallback;
        }
    }

    bool found = false;
    config_pair_t* pair = NULL;
    LIST_FOR_EACH(pair, &targetSection->pairs, entry)
    {
        if (strcasecmp(pair->key, key) == 0)
        {
            found = true;
            break;
        }
    }

    if (!found)
    {
        return fallback;
    }

    return pair->value;
}

int64_t config_int_get(config_t* cfg, const char* section, const char* key, int64_t fallback)
{
    if (cfg == NULL || section == NULL || key == NULL)
    {
        return fallback;
    }

    const char* str = config_string_get(cfg, section, key, NULL);
    if (str == NULL)
    {
        return fallback;
    }

    if (strlen(str) >= 3 && str[0] == '0' && (str[1] == 'x' || str[1] == 'X'))
    {
        return strtoll(&str[2], NULL, 16);
    }
    else
    {
        return strtoll(&str[0], NULL, 10);
    }
}

bool config_bool_get(config_t* cfg, const char* section, const char* key, bool fallback)
{
    if (cfg == NULL || section == NULL || key == NULL)
    {
        return fallback;
    }

    const char* str = config_string_get(cfg, section, key, NULL);
    if (str == NULL)
    {
        return fallback;
    }

    if (strcasecmp(str, "true") == 0)
    {
        return true;
    }
    else if (strcasecmp(str, "false") == 0)
    {
        return false;
    }

    return fallback;
}

config_array_t* config_array_get(config_t* cfg, const char* section, const char* key)
{
    const char* str = config_string_get(cfg, section, key, NULL);
    if (str == NULL)
    {
        return NULL;
    }

    config_array_t* array = malloc(sizeof(config_array_t));
    if (array == NULL)
    {
        return NULL;
    }

    uint64_t strLen = strlen(str);

    array->strings = malloc(sizeof(char*) * strLen); // Assume worst case
    array->length = 0;

    if (array->strings == NULL)
    {
        free(array);
        return NULL;
    }

    char* temp = malloc(strLen + 1);
    if (temp == NULL)
    {
        free(array->strings);
        free(array);
        return NULL;
    }
    strcpy(temp, str);

    char* token = strtok(temp, ",");
    while (token != NULL)
    {
        while (isspace(*token))
        {
            token++;
        }

        uint64_t tokenLen = strlen(token);

        while (tokenLen != 0 && isspace(token[tokenLen - 1]))
        {
            tokenLen--;
            token[tokenLen] = '\0';
        }

        if (tokenLen > 0)
        {
            array->strings[array->length] = malloc(tokenLen + 1);
            if (array->strings[array->length] == NULL)
            {
                for (uint64_t i = 0; i < array->length; i++)
                {
                    free(array->strings[i]);
                }
                free(array->strings);
                free(array);
                free(temp);
                return NULL;
            }
            strcpy(array->strings[array->length], token);
            array->length++;
        }
        token = strtok(NULL, ",");
    }

    free(temp);
    return array;
}

void config_array_free(config_array_t* array)
{
    if (array == NULL)
    {
        return;
    }

    for (uint64_t i = 0; i < array->length; i++)
    {
        free(array->strings[i]);
    }
    free(array->strings);
    free(array);
}

uint64_t config_array_length(config_array_t* array)
{
    return array->length;
}

const char* config_array_string_get(config_array_t* array, uint64_t index, const char* fallback)
{
    if (index >= array->length)
    {
        return fallback;
    }

    return array->strings[index];
}

int64_t config_array_int_get(config_array_t* array, uint64_t index, int64_t fallback)
{
    if (index >= array->length)
    {
        return fallback;
    }

    return atoll(array->strings[index]);
}

bool config_array_bool_get(config_array_t* array, uint64_t index, bool fallback)
{
    if (index >= array->length)
    {
        return fallback;
    }

    if (strcasecmp(array->strings[index], "true") == 0)
    {
        return true;
    }
    else if (strcasecmp(array->strings[index], "false") == 0)
    {
        return false;
    }

    return fallback;
}