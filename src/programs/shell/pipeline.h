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
    bool shouldCloseStdin;
    bool shouldCloseStdout;
    bool shouldCloseStderr;
} cmd_t;

typedef struct
{
    cmd_t* cmds;
    uint64_t capacity;
    uint64_t amount;
    int status;
} pipeline_t;

uint64_t pipeline_init(pipeline_t* pipeline, const char* cmdline);

void pipeline_deinit(pipeline_t* pipeline);

uint64_t pipeline_execute(pipeline_t* pipeline);
