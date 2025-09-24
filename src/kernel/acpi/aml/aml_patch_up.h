#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "acpi/aml/aml_node.h"
#include "encoding/arg.h"
#include "encoding/data_integers.h"
#include "encoding/local.h"
#include "encoding/term.h"
#include "log/log.h"

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
 * Anyway, the way ive choosen to do this is to use a "Patch-up" system. When we retrieve a node, both if it is or isent
 * defined, its always retrieved as an "ObjectReference", basically a node storing a pointer to another node. If the
 * node is not defined yet, the pointer is set to NULL and the ObjectReference node is added to a global list of
 * "unresolved references" along with information like where the retrival in the namespace tree started from and the
 * NameString that we attempted to find a node at.
 *
 * Now we can just wait until we find a matching node and patch it, right? Well... consider this, how do we know that
 * we are resolving to the right node? Say we are in \_SB.FOO and want to resolve BAR but its not defined. Due to the
 * NameString parent behaviour discussed perviously, BAR could be either att \_SB.FOO.BAR, \_SB.BAR or \BAR. Now say we
 * later define \_SB.BAR, we would then try to patch all references to bar that can reach \_SB.BAR, but what if we later
 * defined \_SB.FOO.BAR? Well that would mean we resolved to the wrong node. This also leads to the realization that its
 * actually impossible to resolve any node ever!
 *
 * Lets go back and say that a \_SB.BAR was defined when we originally tried to resolve BAR when we were in \_SB.FOO, we
 * would then resolve to \_SB.BAR. But what if we later defined \_SB.FOO.BAR? Well then we resolved to the wrong node!
 *
 * In that case the solution must just be a two pass system, right? Nope. Then we also have issues since the type and
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

/** @} */
