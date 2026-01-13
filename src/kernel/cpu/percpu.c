#include <kernel/cpu/percpu.h>

#include <kernel/cpu/cpu.h>
#include <kernel/log/panic.h>
#include <kernel/mem/vmm.h>
#include <kernel/sync/lock.h>
#include <string.h>

#define PERCPU_MAX_SECTIONS 128

static BITMAP_CREATE_ZERO(allocated, CONFIG_PERCPU_SIZE / PERCPU_ALIGNMENT);

typedef struct percpu_section
{
    percpu_def_t* start;
    percpu_def_t* end;
    uint64_t generation;
    bool dying;
} percpu_section_t;

/// We cant do memory allocation during early boot so we have to statically allocate the percpu sections.
static percpu_section_t sections[PERCPU_MAX_SECTIONS];
static size_t sectionCount = 0;

static uint64_t globalGeneration = 0;
static uint64_t globalAck = 0;

static PERCPU_DEFINE(uint64_t, pcpu_generation);
static PERCPU_DEFINE(uint64_t, pcpu_ack);

static lock_t lock = LOCK_CREATE();

percpu_t percpu_alloc(size_t size)
{
    size = ROUND_UP(size, PERCPU_ALIGNMENT);

    LOCK_SCOPE(&lock);

    uint64_t offset = bitmap_find_clear_region_and_set(&allocated, 0, allocated.length, size / PERCPU_ALIGNMENT, 1);
    if (offset == allocated.length)
    {
        errno = ENOMEM;
        return ERR;
    }

    cpu_t* cpu;
    CPU_FOR_EACH(cpu)
    {
        uintptr_t addr = (uintptr_t)cpu->percpu + (offset * PERCPU_ALIGNMENT);
        memset((void*)addr, 0, size);
        assert(cpu->self != NULL);
    }

    return offsetof(cpu_t, percpu) + (offset * PERCPU_ALIGNMENT);
}

void percpu_free(percpu_t ptr, size_t size)
{
    if (ptr == ERR)
    {
        return;
    }
    size = ROUND_UP(size, PERCPU_ALIGNMENT);

    LOCK_SCOPE(&lock);

    uint64_t offset = (ptr - offsetof(cpu_t, percpu)) / PERCPU_ALIGNMENT;
    bitmap_clear_range(&allocated, offset, offset + (size / PERCPU_ALIGNMENT));
}

static void percpu_run_ctors(percpu_section_t* section)
{
    for (percpu_def_t* def = section->start; def < section->end; def++)
    {
        if (def->ctor != NULL)
        {
            def->ctor();
        }
    }
}

static void percpu_run_dtors(percpu_section_t* section)
{
    for (percpu_def_t* def = section->start; def < section->end; def++)
    {
        if (def->dtor != NULL)
        {
            def->dtor();
        }
    }
}

void percpu_update(void)
{
    LOCK_SCOPE(&lock);

    if (*pcpu_generation < globalGeneration)
    {
        for (size_t i = 0; i < sectionCount; i++)
        {
            percpu_section_t* section = &sections[i];
            if (section->generation > *pcpu_generation && !section->dying)
            {
                percpu_run_ctors(section);
            }
        }
        *pcpu_generation = globalGeneration;
    }

    if (*pcpu_ack < globalAck)
    {
        for (size_t i = 0; i < sectionCount; i++)
        {
            percpu_section_t* section = &sections[i];
            if (section->generation > *pcpu_ack && section->dying)
            {
                percpu_run_dtors(section);
            }
        }
        *pcpu_ack = globalAck;
    }
}

void percpu_init_section(percpu_def_t* start, percpu_def_t* end)
{
    for (percpu_def_t* percpu = start; percpu < end; percpu++)
    {
        *percpu->ptr = percpu_alloc(percpu->size);
        if (*percpu->ptr == (percpu_t)ERR)
        {
            panic(NULL, "failed to allocate percpu variable");
        }
    }

    lock_acquire(&lock);
    if (sectionCount >= PERCPU_MAX_SECTIONS)
    {
        panic(NULL, "too many percpu sections");
    }

    percpu_section_t* section = &sections[sectionCount++];
    section->start = start;
    section->end = end;
    section->dying = false;

    globalGeneration++;
    section->generation = globalGeneration;
    lock_release(&lock);

    percpu_update();
}

void percpu_finit_section(percpu_def_t* start, percpu_def_t* end)
{
    percpu_section_t* target = NULL;

    lock_acquire(&lock);

    for (size_t i = 0; i < sectionCount; i++)
    {
        percpu_section_t* section = &sections[i];
        if (section->start == start)
        {
            target = section;
            break;
        }
    }

    if (target == NULL)
    {
        lock_release(&lock);
        return;
    }

    target->dying = true;
    globalAck++;
    lock_release(&lock);

    percpu_update();

    bool allAcked = false;
    while (!allAcked)
    {
        allAcked = true;

        cpu_t* cpu;
        CPU_FOR_EACH(cpu)
        {
            uint64_t* cpuAck = CPU_PTR(cpu->id, pcpu_ack);
            if (*cpuAck < globalAck)
            {
                allAcked = false;
                break;
            }
        }
        if (!allAcked)
        {
            asm volatile("pause");
        }
    }

    lock_acquire(&lock);

    for (size_t i = 0; i < sectionCount; i++)
    {
        if (sections[i].start == start)
        {
            memmove(&sections[i], &sections[i + 1], (sectionCount - i - 1) * sizeof(percpu_section_t));
            sectionCount--;
            break;
        }
    }

    lock_release(&lock);

    for (percpu_def_t* percpu = start; percpu < end; percpu++)
    {
        percpu_free(*percpu->ptr, percpu->size);
        *percpu->ptr = 0;
    }
}