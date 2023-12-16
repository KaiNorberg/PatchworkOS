#pragma once

#include "virtual_memory/virtual_memory.h"
#include "interrupts/interrupts.h"

typedef struct 
{
    InterruptStackFrame State;

    uint64_t StackBottom;
    uint64_t StackTop;
} TaskContext;

TaskContext* context_new(void* instructionPointer, uint64_t codeSegment, uint64_t stackSegment, uint64_t rFlags);

void context_free(TaskContext* context);

void context_save(TaskContext* context, const InterruptStackFrame* state);

void context_load(const TaskContext* context, InterruptStackFrame* state);