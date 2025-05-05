#pragma once

#include <sys/io.h>
#include <sys/list.h>

typedef struct
{
    const char** argv;
    uint64_t argc;
    fd_t stdin;
    fd_t stdout;
    fd_t stderr;
    bool closeStdin;
    bool closeStdout;
    bool closeStderr;
} cmd_t;

typedef struct
{
    cmd_t* cmds;
    uint64_t capacity;
    uint64_t amount;
} pipeline_t;

uint64_t pipeline_init(pipeline_t* pipeline, const char* cmdline);

void pipeline_deinit(pipeline_t* pipeline);

void pipeline_execute(pipeline_t* pipeline);
