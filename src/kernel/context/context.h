#pragma once

#include "page_directory/page_directory.h"
#include "interrupts/interrupts.h"

typedef struct 
{
    InterruptStackFrame state;

    uint64_t stackBottom;
    uint64_t stackTop;
} Context;

Context* context_new(void* instructionPointer, uint64_t codeSegment, uint64_t stackSegment, uint64_t rFlags);

void context_free(Context* context);

void context_save(Context* context, const InterruptStackFrame* state);

void context_load(const Context* context, InterruptStackFrame* state);