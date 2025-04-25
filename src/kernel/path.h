#pragma once

#include <stdint.h>
#include <sys/io.h>
#include <sys/node.h>

#define PATH_NAME_SEPARATOR '/'
#define PATH_LABEL_SEPARATOR ':'

#define PATH_VALID_CHAR(ch) (isalnum((ch)) || strchr("_-. ()[]{}~!@#$%^&',;=+", (ch)))

#define PATH_END_OF_NAME(ch) ((ch) == PATH_NAME_SEPARATOR || (ch) == '\0')
#define PATH_END_OF_LABEL(ch) ((ch) == PATH_LABEL_SEPARATOR || (ch) == '\0')

#define PATH_NAME_IS_DOT(name) ((name)[0] == '.' && PATH_END_OF_NAME((name)[1]))
#define PATH_NAME_IS_DOT_DOT(name) ((name)[0] == '.' && (name)[1] == '.' && PATH_END_OF_NAME((name)[2]))

typedef struct path
{
    char volume[MAX_NAME];
    char buffer[MAX_PATH]; // Format: [name]\0[name]\0...\0\3
} path_t;

#define PATH_FOR_EACH(name, path) \
    for ((name) = (path)->buffer; (name)[0] != '\3' && (name)[1] != '\3'; (name) += strlen((name)) + 1)

uint64_t path_init(path_t* path, const char* string, path_t* cwd);

void path_to_string(const path_t* path, char* dest);

node_t* path_traverse_node(const path_t* path, node_t* node);

bool path_valid_name(const char* name);
