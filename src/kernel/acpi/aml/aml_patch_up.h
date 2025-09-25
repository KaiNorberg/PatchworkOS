#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <sys/list.h>

typedef struct aml_node aml_node_t;

/**
 * @brief Patch-up system for forward references.
 * @defgroup kernel_acpi_aml_patch_up Patch-up
 * @ingroup kernel_acpi_aml
 *
 * This module is the reason everyone hates ACPI. We need to support forward references, such that any object referenced
 * might not yet be defined or declared. Why couldent they just add forward declarations like C? No clue, but hey, they
 * are highly educated Intel engineers so what do I know.
 *
 * There are many ways of handling this, all of them equally problematic. Here are a few of the issues that make forward
 * references so bad:
 * - If a attempt to resolve a NameString fails the interpreter will then try to find the object in the parent scope,
 * this means that if a object is not yet defined (its a forward reference) we have no way to know where exactly this
 * object will end up. It could be in the current scope, in the parent scope, or in any ancestor scope.
 * - Its possible for two objects to have the same name, so we cant use that to identify a forward reference.
 * - The object might never be defined, in which case we need to error out at some point.
 * - Dont get me started on RefOf and CondRefOf.
 * - And so much more... When you go over everything you will arive at the conclusion that no matter what solution
 * you choose there will always be situations where there will be, at best, undefined behavior.
 *
 * Anyway, the way ive choosen to do this is to use a "Patch-up" system. When we attempt to retrieve a node that is not
 * yet defined we will get a node of type `AML_DATA_UNRESOLVED` (this behaviour can be disabled when needed) this node
 * stores information like where the retrival started from in the namespace tree and the NameString that we attempted to
 * resolve. We can then add this node to a global list of unresolved nodes along with a callback that will be
 * called when a matching node is found. The callback can then patch the unresolved node in whatever way it wants, for
 * example converting its type before storing it.
 *
 * Now we can just wait until we find a matching node, call the callback and patch it, right? Well... consider this, how
 * do we know that we are resolving to the right node? Say we are in \_SB.FOO and want to resolve BAR but its not
 * defined. Due to the NameString parent behaviour discussed perviously, BAR could be either att \_SB.FOO.BAR, \_SB.BAR
 * or \BAR. Now say we later define \_SB.BAR, we would then try to patch all the relavent nodes that can reach \_SB.BAR,
 * but what if we later defined \_SB.FOO.BAR? Well that would mean we resolved to the wrong node. This also leads to the
 * realization that its actually impossible to resolve any node ever!
 *
 * Lets go back and say that a \_SB.BAR was defined when we originally tried to resolve BAR when we were in \_SB.FOO, we
 * would then resolve to \_SB.BAR. But what if we later defined \_SB.FOO.BAR? Well then we resolved to the wrong node!
 * All becouse of the combination of forward references and the parent scope search behaviour.
 *
 * The solution must just be a two pass system, right? Nope. Then we also have issues since the type and
 * location of objects is not as simple as something like JSON, they are defined dynamically, so the only way to know
 * exactly where everything will be is to parse the entire AML bytecode which we cant do becouse we need the forward
 * references to do that. So we are back to square one. We could ignore the forward refences during the first pass, but
 * then we would have lots of undefined behaviour evaluating certain objects that depend on other objects.
 *
 * One more idea is lazy evaluation, where we only resolve the forward references when they are actually used. This
 * would then lead to unpredictable behaviour as the resolved object would depend on the time of evaluation, which would
 * just be confusing even if the first resolution was cached.
 *
 * So... essay (rant) over. The point is that the "Patch-up" system described here isent perfect, but its probably the
 * best we can do. All behaviour is "defined" in the sense that it will always do the same thing, but its not
 * guaranteed to be the "right" thing as thats impossible. In practice it seems to work well enough.
 *
 * @{
 */

/**
 * @brief Callback type for resolving a forward reference.
 *
 * Takes the now matched node and the previously unresolved node as parameters. The callback should patch
 * the unresolved node in whatever way it wants, for example performing type conversion or similar.
 */
typedef uint64_t (*aml_patch_up_resolve_callback_t)(aml_node_t* match, aml_node_t* unresolved);

/**
 * @brief Entry in the global list of unresolved references.
 * @struct aml_patch_up_entry_t
 */
typedef struct aml_patch_up_entry
{
    list_entry_t entry;                       //!< List entry for the global list of unresolved references.
    aml_node_t* unresolved;                   //!< The unresolved node.
    aml_patch_up_resolve_callback_t callback; //!< The callback to call when a matching node is found.
} aml_patch_up_entry_t;

/**
 * @brief Initialize the patch-up system.
 */
void aml_patch_up_init(void);

/**
 * @brief Adds a unresolved reference to the global list.
 *
 * @param unresolved The unresolved node to add, must be of type `AML_DATA_UNRESOLVED` and allocated (as in created
 * using `aml_node_new()`).
 * @param callback The callback to call when a matching node is found.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_patch_up_add_unresolved(aml_node_t* unresolved, aml_patch_up_resolve_callback_t callback);

/**
 * @brief Removes an unresolved reference from the global list.
 *
 * @param unresolved The unresolved node to remove.
 */
void aml_patch_up_remove_unresolved(aml_node_t* unresolved);

/**
 * @brief Attempts to resolve all unresolved references.
 *
 * TODO: I am still not sure when would be the best time to call this function, for now its called after the DSDT and
 * all SSDTs have been loaded, i am quite sure that we will end up getting issues with unresolved references due to
 * this, but instead of trying to solve that now, we will just fix it as issues arise.
 *
 * Note that a failure to resolve a node is not considered an error, the function will just continue
 * to the next unresolved reference.
 *
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_patch_up_resolve_all(void);

/**
 * @brief Get the number of unresolved references in the global list.
 *
 * @return The number of unresolved references.
 */
uint64_t aml_patch_up_unresolved_count(void);

/** @} */
