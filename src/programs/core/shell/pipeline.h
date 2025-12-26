#pragma once

#include <sys/io.h>
#include <sys/list.h>
#include <sys/proc.h>

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
    pid_t pid;
} cmd_t;

typedef struct
{
    cmd_t* cmds;
    uint64_t capacity;
    uint64_t amount;
    char status[MAX_PATH];
} pipeline_t;

uint64_t pipeline_init(pipeline_t* pipeline, const char* cmdline, fd_t stdin, fd_t stdout, fd_t stderr);

void pipeline_deinit(pipeline_t* pipeline);

void pipeline_execute(pipeline_t* pipeline);

void pipeline_wait(pipeline_t* pipeline);