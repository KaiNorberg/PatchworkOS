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
 * vice versa. We can also use this to resolve symbols between modules.
 *
 * In the end we have a large structure of all currently loaded symbols in the kernel or modules, and we can search this
 * structure by name or by address.
 *
 * ## The Structure
 *
 * The kernel stores symbols using three main structures, which when combined form a slightly over optimized way to
 * retrieve symbols by name or address and to easily remove symbols when a module is unloaded.
 *
 * The structures are:
 * - A id-keyed map of symbol groups (`symbol_group_t`), used to group symbols for easy removal later.
 * - A name-keyed map of symbol names (`symbol_name_t`), used to resolve names to addresses.
 * - An addr-sorted array of symbol addresses (`symbol_addr_t`), used to resolve addresses to names using binary search.
 *
 * These structures form a kind of circular graph, where from a group we can retrieve the names, from the names we can
 * retrieve the addresses and from the addresses we can retrieve the group again. Its also possible to go from a address to its name using the `CONTAINER_OF` macro.
 *
 * Note that we cant use a map for the addresses as we need to be able to find non-exact matches when resolving an
 * address. If a address inside a function is provided we still want to be able to resolve it to the function name, this
 * is done by finding the closest symbol with an address less than or equal to the provided address.
 *
 * @{
 */

/**
 * @brief Maximum length of a symbol name.
 */
#define SYMBOL_MAX_NAME MAP_KEY_MAX_LENGTH

/**
 * @brief Symbol group identifier type.
 * @typedef symbol_group_id_t
 *
 * Used to easily group symbols for removal later, mostly used by modules to remove all their symbols when unloaded.
 *
 * A value of `0` indicates that its part of the kernel and not a module.
 */
typedef uint64_t symbol_group_id_t;

/**
 * @brief Symbol group structure.
 * @typedef symbol_group_t
 *
 * Stored in a id-keyed map.
 */
typedef struct
{
    map_entry_t entry;
    symbol_group_id_t id;
    list_t names;
} symbol_group_t;

/**
 * @brief Symbol name mapping structure.
 * @struct symbol_name_t
 *
 * Stored in a name-keyed map for name to address resolution.
 */
typedef struct
{
    list_entry_t groupEntry;
    map_entry_t mapEntry;
    list_t addrs;
    char name[SYMBOL_MAX_NAME];
} symbol_name_t;

/**
 * @brief Symbol address mapping structure.
 * @struct symbol_addr_t
 *
 * Stored in a addr-sorted array for address to name resolution using binary search and in the relevant
 * `symbol_name_t`'s address list for name to address resolution.
 */
typedef struct
{
    list_entry_t nameEntry;
    void* addr;
    symbol_group_id_t groupId;
    Elf64_Symbol_Binding binding;
    Elf64_Symbol_Type type;
} symbol_addr_t;

/**
 * @brief Symbol information structure.
 * @struct symbol_info_t
 *
 * Used to return symbol information from resolution functions.
 */
typedef struct
{
    char name[SYMBOL_MAX_NAME];
    void* addr;
    symbol_group_id_t groupId;
    Elf64_Symbol_Binding binding;
    Elf64_Symbol_Type type;
} symbol_info_t;

/**
 * @brief Generate a unique symbol group identifier.
 *
 * All identifiers are generated sequentially.
 *
 * @return The symbol group identifier.
 */
symbol_group_id_t symbol_generate_group_id(void);

/**
 * @brief Load all kernel symbols from the bootloader provided kernel ELF file.
 *
 * Will panic on failure.
 *
 * @param kernel The bootloader provided kernel information.
 */
void symbol_load_kernel_symbols(const boot_kernel_t* kernel);

/**
 * @brief Add a symbol to the kernel symbol table.
 *
 * Symbols of binding `STB_GLOBAL` must have unique names but can have duplicated addresses, symbols of other bindings can be duplicated in name, address or both.
 *
 * If the symbol is not of type `STT_OBJECT` or `STT_FUNC` the function is a no-op and returns success.
 *
 * @param name The name of the symbol.
 * @param addr The address of the symbol.
 * @param groupId The group identifier of the symbol.
 * @param binding The binding of the symbol, specifies visibility and linkage.
 * @param type The type of the symbol, specifies what the symbol represents.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t symbol_add(const char* name, void* addr, symbol_group_id_t groupId, Elf64_Symbol_Binding binding,
    Elf64_Symbol_Type type);

/**
 * @brief Remove all symbols from the kernel symbol table in the given group.
 *
 * @param groupId The group identifier of the symbols to remove.
 */
void symbol_remove_group(symbol_group_id_t groupId);

/**
 * @brief Resolve a symbol by address.
 *
 * The resolved symbol is the closest symbol with an address less than or equal to the given address. The
 * `outSymbol->addr` will be the address of the symbol, not the given address.
 *
 * If multiple symbols exist at the same address, one of them will be returned, but which one is undefined. Dont rely on
 * this behaviour being predictable.
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
