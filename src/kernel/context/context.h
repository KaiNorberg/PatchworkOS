#pragma once

#include "page_directory/page_directory.h"
#include "interrupts/interrupts.h"

typedef struct 
{
    InterruptStackFrame state;
} Context;

Context* context_new(void* instructionPointer, void* stackPointer, uint64_t codeSegment, uint64_t stackSegment, uint64_t rFlags, PageDirectory* pageDirectory);

void context_free(Context* context);

void context_save(Context* context, const InterruptStackFrame* state);

void context_load(const Context* context, InterruptStackFrame* state);