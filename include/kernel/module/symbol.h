#pragma once

#include <kernel/utils/map.h>

#include <boot/boot_info.h>

#include <sys/elf.h>
#include <sys/list.h>

/**
 * @brief Kernel Symbol Resolution and Management.
 * @defgroup kernel_module_symbol Kernel Symbols
 * @ingroup kernel_module
 *
 * ## What are Symbols?
 *
 * All binary files are made up of "symbols", a name associated with an address in the binary, which includes the
 * kernel. These symbols are usually stored in the binary file of whatever binary we are dealing with, usually the only
 * purpose of these symbols is linking and debugging.
 *
 * ## Runtime Symbol Resolution
 *
 * We can take advantage of these symbols to resolve symbol names to addresses ("kmain" -> 0xXXXXXXXX) and addresses to
 * symbol names (0xXXXXXXXX -> "kmain") at runtime. This is not only massively useful for debugging and logging, but
 * vital for implementing kernel modules, as the kernel effectively acts as a "runtime linker" for the kernel module
 * binaries, resolving any kernel symbols (which are stored in the module binary by its name since it cant know the
 * address beforehand) to their actual addresses in the kernel so that the module can call into the kernel and of course
 * vice versa.
 *
 * In the end we have a large set of all currently loaded symbols in the kernel or modules, and we can search this set
 * by name or by address.
 *
 * @{
 */

/**
 * @brief Maximum length of a symbol name.
 */
#define SYMBOL_MAX_NAME MAP_KEY_MAX_LENGTH

/**
 * @brief Symbol name mapping structure.
 * @struct symbol_name_t
 *
 * Stored in a name-keyed map for name to address resolution.
 */
typedef struct
{
    map_entry_t fromNameEntry; ///< Map entry for name to symbol mapping
    void* addr;                ///< Address of the symbol
} symbol_name_t;

/**
 * @brief Symbol address mapping structure.
 * @struct symbol_addr_t
 *
 * Stored in a addr-sorted array for address to name resolution using binary search.
 */
typedef struct
{
    void* addr;
    char name[SYMBOL_MAX_NAME];
} symbol_addr_t;

/**
 * @brief Symbol information structure.
 * @struct symbol_info_t
 *
 * Used to return symbol information from resolution functions.
 */
typedef struct
{
    void* addr;
    char name[SYMBOL_MAX_NAME];
} symbol_info_t;

/**
 * @brief Load all kernel symbols from the bootloader provided kernel ELF file.
 *
 * Only non-local symbols are loaded, as in symbols that are globally visible (not `static`).
 *
 * Will panic on failure.
 *
 * @param kernel The bootloader provided kernel information.
 */
void symbol_load_kernel_symbols(const boot_kernel_t* kernel);

/**
 * @brief Add a symbol to the kernel symbol table.
 *
 * @param name The name of the symbol.
 * @param addr The address of the symbol.
 * @return On success, `0`. On failure, `ERR` and `errno`
 */
uint64_t symbol_add(const char* name, void* addr);

/**
 * @brief Remove a symbol from the kernel symbol table by address.
 *
 * @param addr The address of the symbol to remove, must be exact.
 */
void symbol_remove_addr(void* addr);

/**
 * @brief Remove a symbol from the kernel symbol table by name.
 *
 * @param name The name of the symbol to remove.
 */
void symbol_remove_name(const char* name);

/**
 * @brief Resolve a symbol by address.
 *
 * Note that the resolved symbol is the closest symbol with an address less than or equal to the given address. The
 * `outSymbol->addr` will be the address of the symbol, not the given address.
 *
 * @param outSymbol Output pointer to store the resolved symbol information.
 * @param addr The address of the symbol to resolve.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t symbol_resolve_addr(symbol_info_t* outSymbol, void* addr);

/**
 * @brief Resolve a symbol by name.
 *
 * @param outSymbol Output pointer to store the resolved symbol information.
 * @param name The name of the symbol to resolve.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t symbol_resolve_name(symbol_info_t* outSymbol, const char* name);

/** @} */
