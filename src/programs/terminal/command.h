#pragma once

typedef struct
{
    const char* name;
    const char* synopsis;
    const char* description;
    void (*callback)(const char*);
} command_t;

void command_parse(const char* command);
