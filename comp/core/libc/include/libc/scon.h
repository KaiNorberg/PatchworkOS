#ifndef _SYS_SCON_H
#define _SYS_SCON_H 1

#if defined(__cplusplus)
extern "C"
{
#endif

#include <libc/dstr.h>
#include <libc/io.h>
#include <libc/list.h>

/**
 * @brief S-expression CONfig (SCON)
 * @defgroup libc_scon SCON
 * @ingroup libc
 *
 * The `sys/scon.h` header provides definitions for parsing and managing S-expression based configuration files and
 * scripts.
 *
 * The SCON language is designed to be a flexible, simple, and human-readable way to store and manipulate hierarchical
 * data in the form of recursive lists of strings while also allowing for surprisingly fast parsing.
 *
 * It is primarily used for component manifests and system configuration.
 *
 * ## Example
 *
 * ```lisp
 * (component
 *     (description "An example component.")
 *     (author "Kai Norberg")
 *     (license GPLv3)
 *     (launch bin/example)
 *     (dependencies
 *         (libc >= 1.0.0)
 *     )
 *     (capabilities
 *         fb
 *         kbd
 *     )
 * )
 * ```
 *
 * ## Grammar
 *
 * ```
 * scon       := expression
 * expression := '(' list ')'
 * list       := ε | item list
 * item       := atom | expression
 * atom       := unquoted | quoted
 * unquoted   := [^ \f\n\r\t\v()"]+
 * quoted     := '"' [^"]* '"'
 * comment    := ';' [^\n]*
 * ```
 *
 * Note that whitespace, defined by the `isspace()` macro, is used to separate atoms and expressions, but is otherwise
 * trimmed.
 *
 * The first atom in a list is as convention used to specify the meaning of the expression, however the format itself
 * does not enforce this.
 *
 * ## Evaluation
 *
 * A parsed SCON state can optionally be evaluated. This means that everything described below is an optional extension
 * of the language, not a core part of it, and should be considered as such.
 *
 * Evaluation is the process of recursively reducing an expression to its simplest form in a depth-first, left-to-right
 * manner.
 *
 * ### Data Types
 *
 * All SCON expressions evaluate to a single atom or a list of atoms. There are no integers, floats or similar, only
 * integer-shaped or float-shaped atoms.
 *
 * For example, "1234" and "0x5678" are integer-shaped atoms and "3.14" is a float-shaped atom.
 *
 * @note Internally, the `scon_eval()` function will cache the result of evaluations to avoid redundant computations.
 * Meaning that internally types beyond atoms or lists are used but this is completely transparent to the user.
 *
 * ### Atoms
 *
 * An atom evaluates to itself.
 *
 * ### Lists
 *
 * A list is evaluated by first evaluating its first item, if this evaluates to a primitive or a lambda it will be
 * executed with the remaining items in the list as its arguments and with the list being replaced by the result of the
 * evaluation.
 *
 * ### Truthy Items
 *
 * In SCON, an item is considered "truthy" if it is not an empty list and not the atom "0".
 *
 * This means that even atoms such as "false", "null" or lists such as `(0)`, are considered truthy.
 *
 * ### Primitives
 *
 * A primitive is the lowest level of behaviour in SCON, they are, in practice, functions registered in C to the SCON
 * evaluator that can be called from within an expression.
 *
 * ### Lambdas
 *
 * A lambda is a user-defined anonymous function within SCON. They are defined using the `lambda` primitive.
 *
 * ### Variables
 *
 * Variables are used to store and retrieve items within a SCON environment. Variables are defined using the `def`
 * primitive and can be accessed by their name prefixed with a `$`.
 *
 * When a variable is accessed, it evaluates to the item it was defined with.
 *
 * We can also use variables to create a more traditional "function definition" by defining a variable as a lambda:
 *
 * ```lisp
 * (def $add (lambda ($a $b) (+ $a $b)))
 *
 * ($add 1 2) ; Evaluates to "3"
 * ```
 *
 * ### Default Primitives
 *
 * The following primitives are available in the SCON evaluator. All primitives are divided into libraries which can be
 * selectively loaded before evaluation. Users can also register their own primitives using the
 * `scon_register_primitive()` function.
 *
 * #### Core Library
 *
 * - `(quote <item>)`: Returns the item without evaluating it.
 * - `(list <item1> <item2> ...)`: Evaluates all arguments and returns them as a list.
 * - `(lambda (<arg1> <arg2> ...) <body>)`: Returns a user-defined function.
 * - `(if <cond> <then> <else>)`: Evaluates `<then>` if `<cond>` is truthy, otherwise evaluates `<else>`.
 * - `(cond (<c1> <e1>) (<c2> <e2>) ...)`: Evaluates conditions in order, returning the expression of the first truthy
 * condition.
 * - `(and <val1> <val2> ...)`: Evaluates arguments left-to-right. Returns the last truthy value, or "0" if any are
 * falsy. Will stop evaluating arguments as soon as one is falsy.
 * - `(or <val1> <val2> ...)`: Evaluates arguments left-to-right. Returns the first truthy value, or "0" if all are
 * falsy. Will stop evaluating arguments as soon as one is truthy.
 * - `(not <val>)`: Returns "1" if the argument is falsy, otherwise "0".
 * - `(def $<name> <value>)`: Defines a variable with the given name and value in the current scope. Returns nothing.
 * - `(set $<name> <value>)`: Updates the value of an existing variable. Returns nothing.
 * - `(eval <item>)`: Evaluates the item as a SCON expression.
 * - `(assert <cond> <string>)`: Throws an error with the given string if the condition evaluates to falsy.
 * - `(print <item1> <item2> ...)`: Prints the string representation of all arguments to the standard output.
 * - `(error <string>)`: Terminates the SCON evaluation with the given string being set as the error message.
 * - `(apply <lambda> <list>)`: Calls a lambda or primitive using the items of the list as its arguments.
 * - `(do <item1> <item2> ...)`: Evaluates all items in sequence within the current scope and returns the result of the
 * last one.
 *
 * #### Math Library
 *
 * - `(+ <val1> <val2> ...)`: Returns the sum of all arguments.
 * - `(- <val1> <val2> ...)`: Returns the result of subtracting the remaining arguments from the first.
 * - `(* <val1> <val2> ...)`: Returns the product of all arguments.
 * - `(/ <val1> <val2> ...)`: Returns the result of dividing the first argument by the remaining ones.
 * - `(% <val1> <val2>)`: Returns the remainder of the division of the first argument by the second.
 * - `(== <val1> <val2>)`: Returns "1" if the arguments are equal (numerically if both are numbers, otherwise by string
 * comparison), otherwise "0".
 * - `(!= <val1> <val2>)`: Returns "1" if the arguments are not equal, otherwise "0".
 * - `(< <val1> <val2>)`: Returns "1" if the first argument is less than the second, otherwise "0".
 * - `(> <val1> <val2>)`: Returns "1" if the first argument is greater than the second, otherwise "0".
 * - `(<= <val1> <val2>)`: Returns "1" if the first argument is less than or equal to the second, otherwise "0".
 * - `(>= <val1> <val2>)`: Returns "1" if the first argument is greater than or equal to the second, otherwise "0".
 * - `(& <val1> <val2> ...)`: Returns the bitwise AND of all arguments.
 * - `(| <val1> <val2> ...)`: Returns the bitwise OR of all arguments.
 * - `(^ <val1> <val2> ...)`: Returns the bitwise XOR of all arguments.
 * - `(~ <val>)`: Returns the bitwise NOT of the argument.
 * - `(<< <val> <shift>)`: Returns the value bitwise shifted left.
 * - `(>> <val> <shift>)`: Returns the value bitwise shifted right.
 * - `(min <val1> <val2> ...)`: Returns the smallest of all arguments.
 * - `(max <val1> <val2> ...)`: Returns the largest of all arguments.
 * - `(abs <val>)`: Returns the absolute value of the argument.
 * - `(floor <val>)`: Returns the largest integer less than or equal to the argument.
 * - `(ceil <val>)`: Returns the smallest integer greater than or equal to the argument.
 * - `(round <val>)`: Returns the argument rounded to the nearest integer.
 * - `(clamp <val> <min> <max>)`: Restricts a value to be between the given minimum and maximum.
 * - `(pow <base> <exp>)`: Returns the base raised to the power of the exponent.
 * - `(sqrt <val>)`: Returns the square root of the argument.
 * - `(sin <val>)`: Returns the sine of the argument.
 * - `(cos <val>)`: Returns the cosine of the argument.
 * - `(tan <val>)`: Returns the tangent of the argument.
 * - `(asin <val>)`: Returns the arcsine of the argument.
 * - `(acos <val>)`: Returns the arccosine of the argument.
 * - `(atan <val>)`: Returns the arctangent of the argument.
 * - `(atan2 <y> <x>)`: Returns the arctangent of the quotient of its arguments.
 * - `(exp <val>)`: Returns the value of e raised to the power of the argument.
 * - `(log <val>)`: Returns the natural logarithm of the argument.
 * - `(log10 <val>)`: Returns the base-10 logarithm of the argument.
 * - `(rand <min> <max>)`: Returns a random number between the given range.
 * - `(seed <val>)`: Seeds the random number generator.
 *
 * #### IO Library
 *
 * - `(include <path>)`: Returns the result of evaluating the SCON file at the given path, variables defined in the
 * included file will be available in the current scope.
 * - `(read-file <path>)`: Reads the file at the given path and returns its contents as a raw string atom without
 * evaluating it.
 *
 * #### System Library
 *
 * - `(time)`: Returns the current time in seconds since the unix epoch.
 * - `(env <name>)`: Returns the value of the environment variable as an atom, or an empty string if it is not set.
 *
 * #### Item Library
 *
 * - `(len <list>)`: Returns the number of items in a list or the number of characters in an atom.
 * - `(type <item>)`: Returns "atom" if the item is an atom, or "list" if it is a list.
 * - `(atom? <item>)`: Returns "1" if the item is an atom, otherwise "0".
 * - `(list? <item>)`: Returns "1" if the item is a list, otherwise "0".
 * - `(number? <item>)`: Returns "1" if the item is an atom that can be parsed as a valid integer or float, otherwise
 * "0".
 * - `(int? <item>)`: Returns "1" if the item is an atom that can be parsed as a valid integer and not a float,
 * otherwise "0".
 * - `(float? <item>)`: Returns "1" if the item is an atom that can be parsed as a valid float and not an integer,
 * otherwise "0".
 * - `(empty? <item>)`: Returns "1" if the item is an empty list `()` or an empty atom `""`, otherwise "0".
 * - `(eq? <atom1> <atom2>)`: Returns "1" if the atoms are exactly equal as strings, otherwise "0".
 * - `(starts-with? <atom> <prefix>)`: Returns "1" if the atom starts with the prefix, otherwise "0".
 * - `(ends-with? <atom> <suffix>)`: Returns "1" if the atom ends with the suffix, otherwise "0".
 * - `(concat <atom1> <atom2> ...)`: Returns a new atom by concatenating all arguments.
 * - `(format <fmt> <val1> ...)`: Formats a string using standard format specifiers and the provided arguments.
 * - `(first <list>)`: Returns the first item of a list (traditionally `car`).
 * - `(last <list>)`: Returns the last item of a list.
 * - `(rest <list>)`: Returns a list containing all but the first item of the input list.
 * - `(prepend <list> <item>)`: Returns a new allocated list with `<item>` prepended to `<list>`.
 * - `(nth <n> <list>)`: Returns the n-th item of a list.
 * - `(map <lambda> <list>)`: Returns a new list by applying `<lambda>` to each item in `<list>`.
 * - `(filter <lambda> <list>)`: Returns a new list containing only items from `<list>` for which `<lambda>` returns
 * truthy.
 * - `(reduce <lambda> <initial> <list>)`: Reduces `<list>` to a single value by applying `<lambda>` to an accumulator
 * and each item.
 * - `(reverse <list>)`: Returns a new list with the items in reverse order.
 * - `(append <list> <item1> <item2> ...)`: Returns a new list with the provided items added to the end of the list.
 * - `(merge <list1> <list2> ...)`: Returns a new list created by concatenating all provided lists together.
 * - `(slice <list> <start> <end>)`: Returns a sub-list from the `<start>` index to the `<end>` index.
 * - `(join <list> <separator>)`: Returns a new atom created by joining all items in `<list>` with `<separator>`.
 * - `(split <atom> <separator>)`: Returns a new list by splitting `<atom>` into sub-atoms at each occurrence of
 * `<separator>`.
 * - `(sort <list>)`: Returns a new list with the items sorted.
 * - `(find <list> <name>)`: Returns the first sub-list whose first element matches `<name>`.
 * - `(lookup <name> <list>)`: Returns the second item of the first sub-list matching `<name>`.
 * - `(keys <list>)`: Returns a new list containing the first item of every sub-list.
 * - `(contains <list> <item>)`: Returns "1" if `<list>` contains `<item>`, otherwise "0".
 * - `(upper <atom>)`: Returns a new atom with all characters converted to uppercase.
 * - `(lower <atom>)`: Returns a new atom with all characters converted to lowercase.
 * - `(trim <atom>)`: Returns a new atom with leading and trailing whitespace removed.
 * - `(replace <atom> <old> <new>)`: Returns a new atom with all occurrences of `<old>` replaced by `<new>`.
 * - `(substr <atom> <start> <length>)`: Returns a sub-string of the atom starting at `<start>` with the given
 * `<length>`.
 *
 * #### Color Library
 *
 * - `(rgb <r> <g> <b>)`: Returns a atom in the format "0xRRGGBB" representing the color.
 * - `(rgba <r> <g> <b> <a>)`: Returns an atom in the format "0xAARRGGBB" representing the color.
 * - `(hsl <h> <s> <l>)`: Returns an atom in the format "0xRRGGBB" calculated from the HSL values.
 * - `(hsla <h> <s> <l> <a>)`: Returns an atom in the format "0xAARRGGBB" calculated from the HSLA values.
 * - `(mix <color1> <color2> <weight>)`: Returns a new color atom by mixing two colors based on the weight (0.0 to 1.0).
 * - `(invert <color>)`: Returns the inverse of the color. *
 * - `(lighten <color> <amount>)`: Returns a new color atom by lightening the color by the given amount (0.0 to 1.0).
 * - `(darken <color> <amount>)`: Returns a new color atom by darkening the color by the given amount (0.0 to 1.0).
 * - `(alpha <color> <amount>)`: Returns a new color atom by setting or overriding the alpha channel of the color (0.0
 * to 1.0).
 * - `(luma <color>)`: Returns the relative luminance of the color as a value between 0.0 and 1.0.
 *
 * @{
 */

/**
 * @brief SCON item types.
 */
typedef enum
{
    SCON_NONE = 0,
    SCON_ATOM = 1,
    SCON_LIST = 2
} scon_type_t;

/**
 * @brief Hidden SCON item structure.
 * @struct scon_item_t
 */
typedef struct scon_item scon_item_t;

/**
 * @brief SCON state structure.
 * @struct scon_t
 */
typedef struct scon scon_t;

/**
 * @brief Parse a SCON buffer into a SCON structure.
 *
 * @warning Even if this function fails, the SCON structure might still be allocated and must be freed. This is to
 * provide access to error information via `scon_error_msg()`.
 *
 * @param input The buffer to parse.
 * @param size The size of the input buffer.
 * @param out Pointer to the output SCON structure pointer.
 * @return An appropriate status value.
 */
status_t scon_parse(const char* input, size_t size, scon_t** out);

/**
 * @brief Free a the SCON structure.
 *
 * @note Will not free the input buffer.
 *
 * @param scon Pointer to the SCON structure to free.
 */
void scon_free(scon_t* scon);

/**
 * @brief Get the last error message from a SCON structure.
 *
 * @param scon Pointer to the SCON structure.
 * @return A null-terminated string containing the error message.
 */
const char* scon_error_msg(scon_t* scon);

/**
 * @brief Get the root item of the SCON structure.
 *
 * @param scon Pointer to the SCON structure.
 * @return A reference to the root item.
 */
scon_item_t* scon_root(scon_t* scon);

/**
 * @brief Get the next item in the list.
 *
 * @param entry The reference to an item in a list.
 * @return A reference to the next item in the list, or a reference with index `SCON_NONE` if there is no next item.
 */
scon_item_t* scon_next(scon_item_t* entry);

/**
 * @brief Get the first item of a list.
 *
 * @param ref The reference to the list item.
 * @return A reference to the first item in the list, or a reference with index `SCON_NONE` if the list is empty or the
 * item is an atom.
 */
scon_item_t* scon_first(scon_item_t* list);

/**
 * @brief Get the last item of a list.
 *
 * @param list The reference to the list item.
 * @return A reference to the last item in the list, or a reference with index `SCON_NONE` if the list is empty or the
 * item is an atom.
 */
scon_item_t* scon_last(scon_item_t* list);

/**
 * @brief Check if a reference is an atom.
 *
 * @param ref The reference to check.
 * @return `true` if the reference is an atom, `false` otherwise.
 */
bool scon_is_atom(scon_item_t* ref);

/**
 * @brief Check if a reference is a list.
 *
 * @param ref The reference to check.
 * @return `true` if the reference is a list, `false` otherwise.
 */
bool scon_is_list(scon_item_t* ref);

/**
 * @brief Get the string value of an atom.
 *
 * @note The string is not null-terminated. Use the provided length.
 *
 * @param ref The reference to the atom.
 * @param out Output pointer for the string.
 * @param len Output pointer for the length of the string.
 * @return An appropriate status value.
 */
status_t scon_atom_get(scon_item_t* ref, const char** out, size_t* len);

/**
 * @brief Get the length of an atom.
 *
 * @param ref The reference to the atom.
 * @return The length of the atom in bytes.
 */
size_t scon_atom_len(scon_item_t* ref);

/**
 * @brief Get a pointer to the start of an atom's string value.
 *
 * @note The string is not null-terminated. Use `scon_atom_len()` to get the length.
 *
 * @param ref The reference to the atom.
 * @return A pointer to the start of the atom's string value, or `NULL` if the reference is invalid or not an atom.
 */
const char* scon_atom_str(scon_item_t* ref);

/**
 * @brief Compare an atom's value with a string.
 *
 * @param ref The reference to the atom.
 * @param str The string to compare against.
 * @return `true` if the atom matches the string, `false` otherwise.
 */
bool scon_atom_eq(scon_item_t* ref, const char* str);

/**
 * @brief Find a sub-list by its head atom name.
 *
 * Given a list, this searches for a child item that is itself a list whose first element
 * is an atom matching `name`.
 *
 * @param list The reference to the parent list.
 * @param name The name of the head atom to search for.
 * @return A reference to the found list, or a reference with index `SCON_NONE` if not found.
 */
scon_item_t* scon_find(scon_item_t* list, const char* name);

/**
 * @brief Retrieve the n-th item in a SCON list.
 *
 * @param list The reference to the list item.
 * @param n The index of the item to retrieve.
 * @return A reference to the n-th item, or a reference with index `SCON_NONE` if not found.
 */
scon_item_t* scon_get(scon_item_t* list, size_t n);

/**
 * @brief Macro for iterating over the items of a SCON list.
 *
 * @param _item The loop variable, a `scon_item_t*`.
 * @param _list The reference to the lis to iterate over.
 * @param _start The index to start iterating from.
 */
#define SCON_FOR_EACH(_item, _list, _start) \
    for ((_item) = scon_get(_list, _start); (_item) != NULL; (_item) = scon_next(_item))

#if defined(__cplusplus)
}
#endif

#endif

/** @} */