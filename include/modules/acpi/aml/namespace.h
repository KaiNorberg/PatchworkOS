#pragma once

#include <kernel/utils/map.h>
#include <modules/acpi/aml/encoding/name.h>

#include <kernel/defs.h>
#include <stdint.h>
#include <sys/list.h>

/**
 * @brief Namespace and Namespace Overlays
 * @defgroup modules_acpi_aml_namespace Namespace
 * @ingroup modules_acpi_aml
 *
 * We need this slightly complex system as when a method runs it can create named objects that should not be visible
 * outside of the method, and when the method finishes these objects need to be removed. Additionally, if the method
 * calls itself, the new invocation should not see the objects created by the previous invocation. Note that "outside of
 * the method" means that if a inner method is defined inside another method the inner method will see the objects
 * created by the outer method.
 *
 * This is complex enough to need a example, good luck.
 * ```
 * Name (GLOB, 0) // Creates global GLOB with value 0
 *
 * Method (FUNA, 0, NotSerialized)
 * {
 *   Store(30, GLOB) // Updates global GLOB to 10
 *   // Store(0, _VAR) // Error, _VAR does not exist
 * }
 *
 * Method (FUNB, 0, NotSerialized)
 * {
 *   Name (_VAR, GLOB) // Creates _VAR and stores GLOB's value in it
 *   If (IsEqual(_VAR, 20)) // On the second call this will be true as GLOB was updated to 20 by FUNB
 *   {
 *     Return
 *   }
 *   Method (FUNC, 0, NotSerialized)
 *   {
 *      Store(0, _VAR) // Updates _VAR to 0
 *      Store(10, GLOB) // Updates global GLOB to 10
 *   }
 *   FUNC() // Calls inner FUNC
 *   Store(20, GLOB) // Updates global GLOB to 20
 *   FUNB() // Calls itself, will se its own _VAR which will contain the value in GLOB which is now 20
 *   FUNA() // Calls a outer method, cannot see _VAR and would error if it tried to access it
 *   // Method exists here, _VAR is removed
 * }
 * ```
 *
 * This example does not include the edge case where a inner function calls the functions its defined inside of, but
 * it works the same way as calling a outer function.
 *
 * As far as i can tell, this does NOT apply to If, While, or other statements, only methods. So if you
 * create a named object inside an If statement it will be visible outside of the If statement.
 *
 * To solve this we give the `aml_state_t` a `aml_overlay_t` where it can create its named objects. When
 * looking up names we first look in the overlay of the current state and then in the parent overlay and so on until we
 * reach a `NULL` overlay. The last overlay will always be the "global" overlay. When invoking a method it will be given
 * its own `aml_state_t` and its own overlay, the parent of this overlay will be the "highest" overlay that contains the
 * invoked method, so if a inner method is invoked its parent overlay will be the overlay of the outer method. If a
 * outer method is invoked we move up the overlay chain until we find the overlay that contains the method and use that
 * as the parent overlay.
 *
 * Its important to note that overlays are *not* directories they are maps that map a parents id and a childs name to
 * the child object and that when combined form the complete heirarchy. Think of it like this, each overlay defines a
 * incomplete heirarchy of named objects, for example one overlay might define an object `\_FOO._BAR` but the parent
 * object `_FOO` is actually defined in the parent overlay.
 *
 * We also need the ability to commit any names created in a overlay to be globally visible if we are not
 * executing a method but instead parsing a DSDT or SSDT table. This is done using `aml_overlay_commit()`
 * which moves all the overlays objects to its parent overlay, which is usually the global overlay.
 *
 * Note that the term "namespace" in the ACPI specification does not refer to the entire "hierarchy" of named objects
 * but instead to an object that contains named objects, for example a Device object. Yes this is confusing.
 *
 * @see Section 5.3 of the ACPI specification for more details.
 *
 * @{
 */

/**
 * @brief Namespace overlay.
 * @struct aml_overlay_t
 */
typedef struct aml_overlay
{
    map_t map;                  ///< Used to find the children of namespaces using their id and the name of the child.
    list_t objects;             ///< List of all objects in this namespace. Used for fast iteration.
    struct aml_overlay* parent; ///< The parent overlay, or `NULL` if none.
} aml_overlay_t;

/**
 * @brief Name type.
 * @typedef aml_name_t
 *
 * In AML names are just 32-bit values, it just happens that each byte in this value is an ASCII character. So we can
 * optimize things a bit by just treating this as a integer instead of pretending its a string, unless you want to
 * print it for debugging purposes.
 */
typedef uint32_t aml_name_t;

/**
 * @brief Macro to create an `aml_name_t` from 4 characters.
 *
 * @param a First character.
 * @param b Second character.
 * @param c Third character.
 * @param d Fourth character.
 * @return The aml_name_t value.
 */
#define AML_NAME(a, b, c, d) \
    ((aml_name_t)((((aml_name_t)(a) & 0xFF)) | (((aml_name_t)(b) & 0xFF) << 8) | (((aml_name_t)(c) & 0xFF) << 16) | \
        (((aml_name_t)(d) & 0xFF) << 24)))

/**
 * @brief Macro for an undefined name.
 *
 * Real AML never uses lower case letters in names, so we can use 'x' to represent an undefined name.
 */
#define AML_NAME_UNDEFINED AML_NAME('x', 'x', 'x', 'x')

/**
 * @brief Macro to convert an `aml_name_t` to a stack allocated string.
 *
 * @param name The aml_name_t value.
 * @return A stack allocated string representation of the name.
 */
#define AML_NAME_TO_STRING(name) \
    (char[]){((name)) & 0xFF, ((name) >> 8) & 0xFF, ((name) >> 16) & 0xFF, ((name) >> 24) & 0xFF, '\0'}

/**
 * @brief Initialize the namespace heirarchy.
 *
 * @param root The object to use as the root of the namespace heirarchy.
 */
void aml_namespace_init(aml_object_t* root);

/**
 * @brief Expose the entire namespace heirarchy to sysfs.
 *
 * @return On success, `0`. On failure, `ERR`.
 */
uint64_t aml_namespace_expose(void);

/**
 * @brief Get the root object of the namespace heirarchy.
 *
 * @return The root object.
 */
aml_object_t* aml_namespace_get_root(void);

/**
 * @brief Find a child object directly under a parent object in the namespace heirarchy.
 *
 * Will always traverse aliases.
 *
 * @param overlay The overlay to search in, if `NULL` only the global overlay is searched.
 * @param parent The parent scope to search in.
 * @param name The name of the child object to find.
 * @return The object reference or `NULL` if it could not be found.
 */
aml_object_t* aml_namespace_find_child(aml_overlay_t* overlay, aml_object_t* parent, aml_name_t name);

/**
 * @brief Find an object in the namespace heirarchy by name segments.
 *
 * Will always traverse aliases.
 *
 * If there is exactly one name segment, then additional search rules apply meaning that if the object is not found
 * is the parent scope, then we recursively search the parent scope's parent, and so on until we reach the root or
 * find the object.
 *
 * Example:
 * ```c
 * aml_namespace_find(NULL, parent, 2, AML_NAME('A', 'B', 'C', 'D'), AML_NAME('E', 'F', 'G', 'H'));
 * ```
 *
 * @param overlay The overlay to search in, if `NULL` only the global overlay is searched.
 * @param start The scope to start searching from, if `NULL` the search starts from the root object.
 * @param nameCount The number of name segments following.
 * @param ... The name segments of the object to find.
 * @return The object reference or `NULL` if it could not be found.
 */
aml_object_t* aml_namespace_find(aml_overlay_t* overlay, aml_object_t* start, uint64_t nameCount, ...);

/**
 * @brief Find an object in the namespace heirarchy by a name string.
 *
 * Will always traverse aliases.
 *
 * A search through the ACPI namespace follows these rules:
 * - If the NameString starts with a root character (`\`), the search starts from the root object.
 * - If the NameString starts with one or more parent prefix characters (`^`), the search starts from the parent of the
 *    `start` object, moving up one level for each `^`.
 * - If the NameString does not start with a root or parent prefix character, the search starts from the `start` object.
 *    If `start` is `NULL`, the search starts from the root object.
 * - Attempt to find a matching name in the current namespace scope (the `start` object and its children).
 * - If there are no prefixes, only one name segment in the NameString and no match is found in the current scope,
 *  recursively search the parent scope, and so on.
 * - If there are multiple name segments, then recursively searching parent scopes is not allowed. And we just continue
 * searching the next segment in the current scope.
 *
 * @param overlay The overlay to search in, if `NULL` only the global overlay is searched.
 * @param start The scope to start searching from, if `NULL` the search starts from the root object.
 * @param nameString The name string of the object to find.
 * @return The object reference or `NULL` if it could not be found.
 */
aml_object_t* aml_namespace_find_by_name_string(aml_overlay_t* overlay, aml_object_t* start,
    const aml_name_string_t* nameString);

/**
 * @brief Find an object in the namespace heirarchy by a path string.
 *
 * Will always traverse aliases.
 *
 * The path string is a dot separated list of names, for example "ABCD.EFGH.IJKL". Additionally the path can start with
 * a
 * "\" to indicate that the search should start from the root object, or one or more "^" characters to indicate that the
 * search should start from the parent of the `start` object, moving up one level for each "^".
 *
 * If the path does not start with a "\" or "^", the search starts from the `start` object. If `start` is `NULL`, the
 * search starts from the root object.
 *
 * If there is exactly one name segment, then additional search rules apply meaning that if the object is not found
 * is the parent scope, then we recursively search the parent scope's parent, and so on until we reach the root or
 * find the object.
 *
 * @param overlay The overlay to search in, if `NULL` only the global overlay is searched.
 * @param start The scope to start searching from, if `NULL` the search starts from the root object.
 * @param path The path string of the object to find.
 * @return The object reference or `NULL` if it could not be found.
 */
aml_object_t* aml_namespace_find_by_path(aml_overlay_t* overlay, aml_object_t* start, const char* path);

/**
 * @brief Add an child to a parent in the namespace heirarchy.
 *
 * @param overlay The overlay to add the object to, if `NULL` the object is added to the global overlay.
 * @param parent The parent scope to add the object to, if `NULL` the object is added to the root object.
 * @param name The name to give the object.
 * @param object The object to add to the namespace.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_namespace_add_child(aml_overlay_t* overlay, aml_object_t* parent, aml_name_t name, aml_object_t* object);

/**
 * @brief Add an object to the namespace heirarchy using a name string.
 *
 * @param overlay The overlay to add the object to, if `NULL` the object is added to the global overlay.
 * @param start The scope to start searching from to resolve the name string, if `NULL` the search starts from the root
 * object.
 * @param nameString The name string to use to find the parent scope and name of the object.
 * @param object The object to add to the namespace.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_namespace_add_by_name_string(aml_overlay_t* overlay, aml_object_t* start,
    const aml_name_string_t* nameString, aml_object_t* object);

/**
 * @brief Remove an object from the namespace heirarchy it was added to.
 *
 * If the object is not found in the specified overlay or the global namespace heirarchy, nothing happens.
 *
 * The object is dereferenced, so if there are no other references to it, it will be freed.
 *
 * @param object The object to remove from the namespace.
 */
void aml_namespace_remove(aml_object_t* object);

/**
 * @brief Commit all names in a namespace overlay to the global namespace heirarchy.
 *
 * After this call the overlay will be empty.
 *
 * @param overlay The overlay to commit.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_namespace_commit(aml_overlay_t* overlay);

/**
 * @brief Initialize a namespace overlay.
 *
 * Its parent is set to the global overlay.
 *
 * @param overlay The overlay to initialize.
 */
void aml_overlay_init(aml_overlay_t* overlay);

/**
 * @brief Deinitialize a namespace overlay.
 *
 * @param overlay The overlay to deinitialize.
 */
void aml_overlay_deinit(aml_overlay_t* overlay);

/**
 * @brief Set the parent of a namespace overlay.
 *
 * @param overlay The overlay to set the parent of.
 * @param parent The new parent overlay, or `NULL` to set no parent.
 */
void aml_overlay_set_parent(aml_overlay_t* overlay, aml_overlay_t* parent);

/**
 * @brief Search a overlay and its parents for the first overlay that contains the given object.
 *
 * @param overlay The overlay to check.
 * @param object The object to check for.
 * @return On success, the highest overlay that contains the object. On failure, `NULL`.
 */
aml_overlay_t* aml_overlay_find_containing(aml_overlay_t* overlay, aml_object_t* object);

/** @} */
