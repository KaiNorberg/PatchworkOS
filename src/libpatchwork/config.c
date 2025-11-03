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

typedef struct config_pair
{
    char* key;
    char* value;
    list_entry_t entry;
} config_pair_t;

typedef struct config_section
{
    char* name;
    list_entry_t entry;
    list_t pairs;
} config_section_t;

typedef struct config
{
    list_t sections;
} config_t;

static char* config_trim_whitespace(char* str)
{
    char* end;
    while (isspace((unsigned char)*str))
    {
        str++;
    }

    if (*str == 0)
    {
        return str;
    }

    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end))
    {
        end--;
    }

    *(end + 1) = '\0';
    return str;
}

static config_section_t* config_find_section(config_t* cfg, const char* section)
{
    if (cfg == NULL || section == NULL)
    {
        return NULL;
    }

    config_section_t* sec;
    LIST_FOR_EACH(sec, &cfg->sections, entry)
    {
        if (strcasecmp(sec->name, section) == 0)
        {
            return sec;
        }
    }

    return NULL;
}

static config_pair_t* config_find_pair(config_section_t* sec, const char* key)
{
    if (sec == NULL || key == NULL)
    {
        return NULL;
    }

    config_pair_t* pair;
    LIST_FOR_EACH(pair, &sec->pairs, entry)
    {
        if (strcasecmp(pair->key, key) == 0)
        {
            return pair;
        }
    }

    return NULL;
}

config_t* config_open(const char* prefix, const char* name)
{
    if (prefix == NULL || name == NULL)
    {
        return NULL;
    }

    char path[MAX_PATH] = {0};
    snprintf(path, MAX_PATH - 1, "/cfg/%s-%s.cfg", prefix, name);

    FILE* file = fopen(path, "r");
    if (file == NULL)
    {
        return NULL;
    }

    config_t* config = malloc(sizeof(config_t));
    if (config == NULL)
    {
        fclose(file);
        return NULL;
    }
    list_init(&config->sections);

    char lineBuffer[1024];
    config_section_t* currentSection = NULL;
    while (fgets(lineBuffer, sizeof(lineBuffer), file) != NULL)
    {
        char* line = config_trim_whitespace(lineBuffer);
        if (line[0] == '\0' || line[0] == '#' || line[0] == ';')
        {
            continue;
        }

        if (line[0] == '[')
        {
            char* end = strchr(line, ']');
            if (end == NULL)
            {
                continue;
            }
            *end = '\0';
            char* name = config_trim_whitespace(line + 1);
            if (name[0] == '\0')
            {
                continue;
            }

            config_section_t* section = malloc(sizeof(config_section_t));
            if (section == NULL)
            {
                goto error;
            }
            section->name = strdup(name);
            if (section->name == NULL)
            {
                free(section);
                goto error;
            }
            list_entry_init(&section->entry);
            list_init(&section->pairs);

            list_push_back(&config->sections, &section->entry);
            currentSection = section;
        }
        else
        {
            char* equals = strchr(line, '=');
            if (equals == NULL || currentSection == NULL)
            {
                continue;
            }
            *equals = '\0';
            char* key = config_trim_whitespace(line);
            char* value = config_trim_whitespace(equals + 1);
            if (key[0] == '\0')
            {
                continue;
            }

            config_pair_t* pair = malloc(sizeof(config_pair_t));
            if (pair == NULL)
            {
                goto error;
            }
            pair->key = strdup(key);
            pair->value = strdup(value);
            if (pair->key == NULL || pair->value == NULL)
            {
                free(pair->key);
                free(pair->value);
                free(pair);
                goto error;
            }
            list_entry_init(&pair->entry);

            list_push_back(&currentSection->pairs, &pair->entry);
        }
    }

    fclose(file);
    return config;

error:
    fclose(file);
    config_close(config);
    return NULL;
}

void config_close(config_t* config)
{
    if (config == NULL)
    {
        return;
    }

    while (!list_is_empty(&config->sections))
    {
        config_section_t* sec = CONTAINER_OF(list_pop_first(&config->sections), config_section_t, entry);

        while (!list_is_empty(&sec->pairs))
        {
            config_pair_t* pair = CONTAINER_OF(list_pop_first(&sec->pairs), config_pair_t, entry);
            free(pair->key);
            free(pair->value);
            free(pair);
        }

        free(sec->name);
        free(sec);
    }

    free(config);
}

const char* config_get_string(config_t* config, const char* section, const char* key, const char* fallback)
{
    if (config == NULL || section == NULL || key == NULL)
    {
        return fallback;
    }

    config_section_t* sec = config_find_section(config, section);
    if (sec == NULL)
    {
        return fallback;
    }

    config_pair_t* pair = config_find_pair(sec, key);
    if (pair == NULL)
    {
        return fallback;
    }

    return pair->value;
}

int64_t config_get_int(config_t* config, const char* section, const char* key, int64_t fallback)
{
    if (config == NULL || section == NULL || key == NULL)
    {
        return fallback;
    }

    const char* str = config_get_string(config, section, key, NULL);
    if (str == NULL)
    {
        return fallback;
    }

    char* end;
    int64_t value = strtoll(str, &end, 0);
    if (end == str || *end != '\0')
    {
        return fallback;
    }

    return value;
}

bool config_get_bool(config_t* config, const char* section, const char* key, bool fallback)
{
    if (config == NULL || section == NULL || key == NULL)
    {
        return fallback;
    }

    const char* str = config_get_string(config, section, key, NULL);
    if (str == NULL)
    {
        return fallback;
    }

    if (strcasecmp(str, "true") == 0 || strcasecmp(str, "yes") == 0 || strcasecmp(str, "on") == 0 ||
        strcmp(str, "1") == 0)
    {
        return true;
    }
    else if (strcasecmp(str, "false") == 0 || strcasecmp(str, "no") == 0 || strcasecmp(str, "off") == 0 ||
        strcmp(str, "0") == 0)
    {
        return false;
    }

    return fallback;
}

config_array_t* config_get_array(config_t* config, const char* section, const char* key)
{
    static config_array_t emptyArray = {.items = NULL, .length = 0};
    if (config == NULL || section == NULL || key == NULL)
    {
        goto return_empty;
    }

    const char* str = config_get_string(config, section, key, NULL);
    if (str == NULL || str[0] == '\0')
    {
        goto return_empty;
    }

    uint64_t length = strlen(str);
    uint64_t maxSize = sizeof(config_array_t) + (length * sizeof(char*)) + length + 1;

    config_array_t* array = malloc(maxSize);
    if (array == NULL)
    {
        return NULL;
    }
    array->items = (char**)(array + 1);
    array->length = 0;

    char* data = (char*)(array->items + length);

    uint64_t index = 0;
    while (*str != '\0')
    {
        while (isspace((unsigned char)*str))
        {
            str++;
        }

        if (*str == '\0')
        {
            break;
        }

        const char* start = str;

        while (*str != '\0' && *str != ',')
        {
            str++;
        }

        const char* end = str;

        while (end > start && isspace((unsigned char)*(end - 1)))
        {
            end--;
        }

        uint64_t len = (end > start) ? (end - start) : 0;
        if (len > 0)
        {
            memcpy(data, start, len);
            data[len] = '\0';

            array->items[index] = data;

            data += len + 1;
            index++;
        }

        if (*str == ',')
        {
            str++;
        }
    }

    array->length = index;
    return array;

return_empty:
    if (false) // to satisfy compiler
    {
    }
    config_array_t* empty = malloc(sizeof(config_array_t));
    if (empty != NULL)
    {
        empty->items = NULL;
        empty->length = 0;
    }
    return empty;
}

void config_array_free(config_array_t* array)
{
    if (array != NULL)
    {
        free(array);
    }
}
