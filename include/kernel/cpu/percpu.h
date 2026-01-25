#pragma once

#include <kernel/cpu/cpu.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Per CPU data.
 * @defgroup kernel_cpu_percpu Per-CPU Data
 * @ingroup kernel_cpu
 *
 * In the x86 architecture the `gs` and `fs` segment registers can be used to access data relative to the address stored
 * in the `MSR_GS_BASE` or `MSR_FS_BASE` MSRs. In AT&T assembly this would look like this:
 *
 * ```
 * mov %gs:0x10, %rax ; Load the value at address in `MSR_GS_BASE` + 0x10 into rax
 * ```
 *
 * This means that, since each cpu has its own `MSR_GS_BASE`, we can store the address of each CPU's structure in its
 * own `MSR_GS_BASE` and then access data within that structure using offsets.
 *
 * Allocating a percpu variable then becomes as simple as allocating an offset within the `percpu` buffer in the CPU
 * structure, and accessing it using the `gs` segment register.
 *
 * @note Its important to be aware of the distinction that the `gs` register does not store an address directly, rather
 * it allows us to access memory relative to the address stored in the `MSR_GS_BASE` MSR. This is why we define Per-CPU
 * variables as offsets within the CPU structure rather than absolute addresses.
 *
 * ## Defining Per-CPU Variables
 *
 * To define a Per-CPU variable use the `PERCPU_DEFINE()` macro. This will add a `percpu_def_t` entry to the `._percpu`
 * section. The PERCPU_INIT()` macro can be used to allocate and initialize all Per-CPU variables defined in the
 * module's `._percpu` section, potentially invoking any needed constructors.
 *
 * @note All percpu variables should use the `pcpu_` prefix for clarity.
 *
 * ## Constructors and Destructors
 *
 * All Per-CPU variables can optionally have constructors (ctor) and destructors (dtor) defined. These will be called on
 * each CPU either during boot, when the CPU is initialized, or via a call to `percpu_update()`.
 *
 * By default, all variables are zero-initialized when allocated.
 *
 * @{
 */

/**
 * @brief The `PERCPU_ALIGNMENT` constant defines the alignment for per-CPU variables.
 *
 * @note This value should be a power of 2.
 */
#define PERCPU_ALIGNMENT 64

/**
 * @brief The type that the compiler uses to store per-CPU variables.
 */
typedef size_t percpu_t;

/**
 * @brief Structure to define a percpu variable.
 * @struct percpu_def_t
 */
typedef struct
{
    percpu_t* ptr;
    size_t size;
    void (*ctor)(void);
    void (*dtor)(void);
} percpu_def_t;

/**
 * @brief Attribute specifying that the variable is an offset into the `GS` segment register.
 * @def PERCPU
 */
#define PERCPU __seg_gs*

/**
 * @brief Macro to access data in the current cpu.
 *
 * Intended to be used as a pointer to the current cpu structure.
 *
 * @warning The value of this macro is not the address of the current cpu structure, to actually retrieve the address
 * use `SELF->self`.
 */
#define SELF ((cpu_t PERCPU)0)

/**
 * @brief Macro to get a pointer to a percpu variable on the current CPU.
 *
 * @param ptr The percpu variable pointer.
 * @return A pointer to the percpu variable for the current CPU.
 */
#define SELF_PTR(ptr) ((void*)((uintptr_t)(SELF->self->percpu) + ((uintptr_t)(ptr) - offsetof(cpu_t, percpu))))

/**
 * @brief Macro to get a pointer to a percpu variable on a specific CPU.
 *
 * @param id The ID of the CPU.
 * @param ptr The percpu variable pointer.
 * @return A pointer to the percpu variable for the specified CPU.
 */
#define CPU_PTR(id, ptr) \
    ((void*)((uintptr_t)(cpu_get_by_id(id)->percpu) + ((uintptr_t)(ptr) - offsetof(cpu_t, percpu))))

/**
 * @brief Macro to define a percpu variable.
 *
 * This macro defines a percpu variable and registers it in the `._percpu` section, so that it can be initialized by
 * `PERCPU_INIT()`.
 *
 * @param type The type of the percpu variable.
 * @param name The name of the percpu variable.
 * @param ... Optional constructor and destructor functions for the percpu variable.
 */
#define PERCPU_DEFINE(type, name, ...) \
    type PERCPU name; \
    static const percpu_def_t __attribute__((used, section("._percpu"))) _percpu##name = {.ptr = (percpu_t*)&(name), \
        .size = sizeof(typeof(*name)), \
        ##__VA_ARGS__}

/**
 * @brief Macro to define a percpu variable with a constructor.
 *
 * This macro defines a percpu variable with a constructor and registers it in the `._percpu` section, so that it can
 * be initialized by `PERCPU_INIT()`.
 *
 * @param type The type of the percpu variable.
 * @param name The name of the percpu variable.
 */
#define PERCPU_DEFINE_CTOR(type, name) \
    static void name##_ctor(void); \
    PERCPU_DEFINE(type, name, .ctor = name##_ctor, .dtor = NULL); \
    static void name##_ctor(void)

/**
 * @brief Macro to define a percpu variable with a destructor.
 *
 * This macro defines a percpu variable with a destructor and registers it in the `._percpu` section, so that it can
 * be deinitialized by `PERCPU_FINIT()`.
 *
 * @param type The type of the percpu variable.
 * @param name The name of the percpu variable.
 */
#define PERCPU_DEFINE_DTOR(type, name) \
    static void name##_dtor(void); \
    PERCPU_DEFINE(type, name, .ctor = NULL, .dtor = name##_dtor); \
    static void name##_dtor(void)

/**
 * @brief Initialize the percpu system.
 *
 * This will setup the `gs` segment register to point to the  CPU structure.
 *
 * @param cpu The CPU to initialize percpu for, should be the current CPU.
 */
void percpu_init(cpu_t* cpu);

/**
 * @brief Allocates a percpu variable.
 *
 * @param size The size of the percpu variable.
 * @return The offset into the `GS` segment register, or `_FAIL` on failure.
 */
percpu_t percpu_alloc(size_t size);

/**
 * @brief Frees a percpu variable.
 *
 * @param ptr The offset into the `GS` segment register.
 * @param size The size of the percpu variable.
 */
void percpu_free(percpu_t ptr, size_t size);

/**
 * @brief Update percpu sections on the current CPU.
 *
 * This will run any pending constructors or destructors for percpu sections.
 */
void percpu_update(void);

/**
 * @brief Register a percpu section and run constructors.
 */
void percpu_section_init(percpu_def_t* start, percpu_def_t* end);

/**
 * @brief Unregister a percpu section and run destructors.
 */
void percpu_section_deinit(percpu_def_t* start, percpu_def_t* end);

/**
 * @brief Initialize all percpu variables within the current modules `.percpu` section.
 */
#define PERCPU_INIT() \
    do \
    { \
        extern percpu_def_t _percpu_start; \
        extern percpu_def_t _percpu_end; \
        percpu_section_init(&_percpu_start, &_percpu_end); \
    } while (0)

/**
 * @brief Deinitialize all percpu variables within the current modules `.percpu` section.
 */
#define PERCPU_DEINIT() \
    do \
    { \
        extern percpu_def_t _percpu_start; \
        extern percpu_def_t _percpu_end; \
        percpu_section_deinit(&_percpu_start, &_percpu_end); \
    } while (0)

/** @} */
