#pragma once

#include <libc/comp.h>
#include <libc/fs.h>
#include <libc/list.h>
#include <libc/map.h>
#include <libc/scon.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct
{
    list_entry_t entry;
    map_entry_t mapEntry;
    char name[MAX_NAME];
    comp_version_t version;
    char* manifest;
    size_t manifestLength;
    scon_t scon;
    bool processed;
} _comp_req_t;

#define COMP_VERSION_BASE (1ULL << 20)

typedef struct
{
    list_t reqs;
    MAP_DEFINE(map, 16);
    char* error;
    size_t errorLength;
} _comp_loader_t;

void _comp_error(_comp_loader_t* loader, const char* format, ...);
void _comp_loader_init(_comp_loader_t* loader, char* error, size_t errorLength);
void _comp_loader_deinit(_comp_loader_t* loader);
status_t _comp_version_parse(const char* version, size_t versionLen, comp_version_t* out);
status_t _comp_add(_comp_loader_t* loader, const char* name, size_t nameLen, const char* version, size_t versionLen,
    _comp_req_t** out);
status_t _comp_load_dependencies(_comp_loader_t* loader);
status_t _comp_cull_orphans(_comp_loader_t* loader, _comp_req_t* main);
status_t _comp_load_capabilities(_comp_loader_t* loader, fd_t root);
status_t _comp_load_union(_comp_loader_t* loader, fd_t root);
