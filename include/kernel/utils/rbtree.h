#pragma once

#include <kernel/defs.h>

#include <stdint.h>
#include <sys/list.h>

typedef struct rbnode rbnode_t;

/**
 * @brief Augmented Red-Black Tree.
 * @defgroup kernel_utils_rbtree Red-Black Tree
 * @ingroup kernel_utils
 *
 * A Red-Black Tree (RBT) is a tree structure that maintains sorted data to allow for efficient insertion, deletion, and
 * lookup operations with a worst case time complexity of `O(log n)`.
 *
 * ## Used As A Sorted Linked List
 *
 * The name "Red-Black Tree" can be a bit confusing. To the user of the tree, it simply acts as a highly optimized
 * sorted linked list.
 *
 * The tree structure allows for more efficient operations compared to a standard linked list (`O(log n)` vs `O(n)`),
 * and the red-black properties ensure that the tree remains balanced, preventing it from degenerating into a linear
 * structure. However, the user of the tree does not need to be concerned with these implementation details.
 *
 * @todo Cache minimum and maximum nodes for `O(1)` access.
 *
 * ## Update Callbacks
 *
 * The tree supports an optional update callback that is called whenever a node is inserted, removed or swapped. This
 * allows for the tree to be "augmented" with additional data. For example, if you wanted to track the global minimum of
 * some value in each node, you could do so by updating the minimum value in the update callback, such that you no
 * longer need to traverse the tree to find the minimum. Very useful for the scheduler.
 *
 * @see [Wikipedia Red-Black Tree](https://en.wikipedia.org/wiki/Red%E2%80%93black_tree) for more information on
 * Red-Black Trees.
 * @see [Earliest Eligible Virtual Deadline
 * First](https://citeseerx.ist.psu.edu/document?repid=rep1&type=pdf&doi=805acf7726282721504c8f00575d91ebfd750564) for
 * the original paper describing EEVDF, contains some usefull information on balanced trees.
 *
 * @{
 */

/**
 * @brief Red-Black Tree Node Colors.
 * @enum rbnode_color_t
 */
typedef enum
{
    RBNODE_RED = 0,
    RBNODE_BLACK = 1
} rbnode_color_t;

/**
 * @brief Red-Black Tree Node Directions.
 * @enum rbnode_direction_t
 *
 * Used to index into the children array of a `rbnode_t`.
 */
typedef enum
{
    RBNODE_LEFT = 0,
    RBNODE_RIGHT = 1,
    RBNODE_AMOUNT = 2,
} rbnode_direction_t;

/**
 * @brief Get the opposite direction (left <-> right).
 *
 * @param direction The direction to get the opposite of.
 * @return The opposite direction.
 */
#define RBNODE_OPPOSITE(direction) ((rbnode_direction_t)(1 - (direction)))

/**
 * @brief Get the direction of a node from its parent.
 *
 * @param node The node to get the direction of.
 * @return The direction of the node from its parent.
 */
#define RBNODE_FROM_PARENT(node) \
    ((rbnode_direction_t)((node)->parent->children[RBNODE_RIGHT] == (node) ? RBNODE_RIGHT : RBNODE_LEFT))

/**
 * @brief Red-Black Tree Node.
 * @struct rbnode_t
 *
 * Should be embedded in the structure to be stored in the tree, such that the parent structure can be retrieved via
 * `CONTAINER_OF()`.
 */
typedef struct rbnode
{
    rbnode_t* parent;
    rbnode_t* children[RBNODE_AMOUNT];
    rbnode_color_t color;
} rbnode_t;

/**
 * @brief Create a Red-Black Tree Node initializer.
 *
 * @return A Red-Black Tree Node initializer.
 */
#define RBNODE_CREATE \
    (rbnode_t) \
    { \
        .parent = NULL, .children = {NULL, NULL}, .color = RBNODE_RED \
    }

/**
 * @brief Comparison function for Red-Black Tree nodes.
 *
 * Should return:
 * - A negative value if `a` is less than `b`.
 * - Zero if `a` is equal to `b`.
 * - A positive value if `a` is greater than `b`.
 *
 * @param a The first node to compare.
 * @param b The second node to compare.
 * @return The comparison result.
 */
typedef int64_t (*rbnode_compare_t)(const rbnode_t* a, const rbnode_t* b);

/**
 * @brief Update function for Red-Black Tree nodes.
 *
 * Called whenever a node is inserted, removed or swapped.
 *
 * @param node The node to update.
 */
typedef void (*rbnode_update_t)(rbnode_t* node);

/**
 * @brief Red-Black Tree.
 * @struct rbtree_t
 */
typedef struct rbtree
{
    rbnode_t* root;
    rbnode_compare_t compare;
    rbnode_update_t update;
} rbtree_t;

/**
 * @brief Initialize a Red-Black Tree.
 *
 * Will not allocate any memory.
 *
 * @param tree The tree to initialize.
 * @param compare The comparison function to use.
 * @param update The update function to use, or `NULL`.
 */
void rbtree_init(rbtree_t* tree, rbnode_compare_t compare, rbnode_update_t update);

/**
 * @brief Rotate a node in the Red-Black Tree.
 *
 * @see https://upload.wikimedia.org/wikipedia/commons/f/f2/Binary_Tree_Rotation_%28animated%29.gif for a animation of
 * tree rotations.
 *
 * @param tree The tree containing the node to rotate.
 * @param node The node to rotate.
 * @param direction The direction to rotate.
 * @return The new root of the rotated subtree.
 */
rbnode_t* rbtree_rotate(rbtree_t* tree, rbnode_t* node, rbnode_direction_t direction);

/**
 * @brief Find the minimum node in a subtree.
 *
 * This is the same as just going as far left as possible.
 *
 * @param node The root of the subtree.
 * @return The minimum node in the subtree.
 */
rbnode_t* rbtree_find_min(rbnode_t* node);

/**
 * @brief Find the maximum node in a subtree.
 *
 * This is the same as just going as far right as possible.
 *
 * @param node The root of the subtree.
 * @return The maximum node in the subtree.
 */
rbnode_t* rbtree_find_max(rbnode_t* node);

/**
 * @brief Swap two nodes in the Red-Black Tree.
 *
 * Needed as the structure is intrusive, so we can't just swap the data.
 *
 * @param tree The tree containing the nodes to swap.
 * @param a The first node to swap.
 * @param b The second node to swap.
 */
void rbtree_swap(rbtree_t* tree, rbnode_t* a, rbnode_t* b);

/**
 * @brief Insert a node into the Red-Black Tree.
 *
 * @param tree The tree to insert into.
 * @param node The node to insert.
 */
void rbtree_insert(rbtree_t* tree, rbnode_t* node);

/**
 * @brief Remove a node from the Red-Black Tree.
 *
 * @param tree The tree to remove from.
 * @param node The node to remove.
 */
void rbtree_remove(rbtree_t* tree, rbnode_t* node);

/**
 * @brief Move the node to its correct position in the Red-Black Tree.
 *
 * Should be called whenever the metric used for comparison changes.
 *
 * @note This function is optimized assuming the common case where the node is already close to its correct position.
 *
 * @param tree The tree containing the node to update.
 * @param node The node to update.
 */
void rbtree_fix(rbtree_t* tree, rbnode_t* node);

/**
 * @brief Check if the Red-Black Tree is empty.
 *
 * @param tree The tree to check.
 * @return `true` if the tree is empty, `false` otherwise.
 */
bool rbtree_is_empty(const rbtree_t* tree);

/**
 * @brief Get the next node in the tree, in predecessor order.
 *
 * @param node The current node.
 * @return The next node in the tree, or `NULL` if `node` is the last node.
 */
rbnode_t* rbtree_next(const rbnode_t* node);

/**
 * @brief Get the previous node in the tree, in predecessor order.
 *
 * @param node The current node.
 * @return The previous node in the tree, or `NULL` if `node` is the first node.
 */
rbnode_t* rbtree_prev(const rbnode_t* node);

/**
 * @brief Iterates over a Red-Black Tree in ascending order.
 *
 * @param elem The loop variable, a pointer to the structure containing the tree node.
 * @param tree A pointer to the `rbtree_t` structure to iterate over.
 * @param member The name of the `rbnode_t` member within the structure `elem`.
 */
#define RBTREE_FOR_EACH(elem, tree, member) \
    for ((elem) = \
             ((tree)->root != NULL ? CONTAINER_OF(rbtree_find_min((tree)->root), typeof(*(elem)), member) : NULL); \
        (elem) != NULL; (elem) = (rbtree_next(&(elem)->member) != NULL \
                                ? CONTAINER_OF(rbtree_next(&(elem)->member), typeof(*(elem)), member) \
                                : NULL))
/**
 * @brief Iterates over a Red-Black Tree in descending order.
 *
 * @param elem The loop variable, a pointer to the structure containing the tree node.
 * @param tree A pointer to the `rbtree_t` structure to iterate over.
 * @param member The name of the `rbnode_t` member within the structure `elem`.
 */
#define RBTREE_FOR_EACH_REVERSE(elem, tree, member) \
    for ((elem) = \
             ((tree)->root != NULL ? CONTAINER_OF(rbtree_find_max((tree)->root), typeof(*(elem)), member) : NULL); \
        (elem) != NULL; (elem) = (rbtree_prev(&(elem)->member) != NULL \
                                ? CONTAINER_OF(rbtree_prev(&(elem)->member), typeof(*(elem)), member) \
                                : NULL))

/** @} */