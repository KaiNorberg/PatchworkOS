#include <kernel/utils/rbtree.h>

#include <assert.h>

void rbtree_init(rbtree_t* tree, rbnode_compare_t compare)
{
    assert(tree != NULL);
    assert(compare != NULL);

    tree->root = NULL;
    tree->size = 0;
    tree->compare = compare;
}

rbnode_t* rbtree_rotate(rbtree_t* tree, rbnode_t* node, rbnode_direction_t direction)
{
    assert(tree != NULL);
    assert(node != NULL);
    assert(direction == RBNODE_LEFT || direction == RBNODE_RIGHT);

    rbnode_direction_t opposite = RBNODE_OPPOSITE(direction);

    rbnode_t* parent = node->parent;
    rbnode_t* newRoot = node->children[opposite];
    assert(newRoot != NULL);
    rbnode_t* newChild = newRoot->children[direction];

    node->children[opposite] = newChild;
    if (newChild != NULL)
    {
        newChild->parent = node;
    }

    newRoot->children[direction] = node;

    newRoot->parent = parent;
    node->parent = newRoot;
    if (parent != NULL)
    {
        if (node == parent->children[RBNODE_RIGHT])
        {
            parent->children[RBNODE_RIGHT] = newRoot;
        }
        else
        {
            parent->children[RBNODE_LEFT] = newRoot;
        }
    }
    else
    {
        tree->root = newRoot;
    }

    return newRoot;
}

static void rbtree_insert_at(rbtree_t* tree, rbnode_t* parent, rbnode_t* node, rbnode_direction_t direction)
{
    assert(node != NULL);
    assert(direction == RBNODE_LEFT || direction == RBNODE_RIGHT);

    node->color = RBNODE_RED;
    node->parent = parent;

    if (parent == NULL)
    {
        tree->root = node;
        return;
    }

    assert(parent->children[direction] == NULL);
    parent->children[direction] = node;

    while (true)
    {
        if (parent->color == RBNODE_BLACK)
        {
            break;
        }

        rbnode_t* grandparent = parent->parent;
        if (grandparent == NULL)
        {
            parent->color = RBNODE_BLACK;
            return;
        }

        assert(grandparent->children[RBNODE_LEFT] == parent || grandparent->children[RBNODE_RIGHT] == parent);
        rbnode_direction_t fromParent = RBNODE_FROM_PARENT(parent);
        rbnode_t* uncle = grandparent->children[RBNODE_OPPOSITE(fromParent)];
        if (uncle == NULL || uncle->color == RBNODE_BLACK)
        {
            if (node == parent->children[RBNODE_OPPOSITE(fromParent)])
            {
                rbtree_rotate(tree, parent, fromParent);
                parent = grandparent->children[fromParent];
            }

            rbtree_rotate(tree, grandparent, RBNODE_OPPOSITE(fromParent));
            parent->color = RBNODE_BLACK;
            grandparent->color = RBNODE_RED;
            break;
        }

        parent->color = RBNODE_BLACK;
        uncle->color = RBNODE_BLACK;
        grandparent->color = RBNODE_RED;
        node = grandparent;

        parent = node->parent;
        if (parent == NULL)
        {
            break;
        }
        assert(parent->children[RBNODE_LEFT] == node || parent->children[RBNODE_RIGHT] == node);
    }
}

void rbtree_insert(rbtree_t* tree, rbnode_t* node)
{
    assert(tree != NULL);
    assert(node != NULL);
    assert(node->parent == NULL);
    assert(node->children[RBNODE_LEFT] == NULL);
    assert(node->children[RBNODE_RIGHT] == NULL);

    rbnode_t* current = tree->root;
    rbnode_t* parent = NULL;

    rbnode_direction_t direction = RBNODE_LEFT;
    while (current != NULL)
    {
        parent = current;
        if (tree->compare(node, current) < 0)
        {
            direction = RBNODE_LEFT;
            current = current->children[RBNODE_LEFT];
        }
        else
        {
            direction = RBNODE_RIGHT;
            current = current->children[RBNODE_RIGHT];
        }
    }

    rbtree_insert_at(tree, parent, node, direction);
    tree->size++;

    if (tree->root != NULL)
    {
        tree->root->color = RBNODE_BLACK;
    }
}

rbnode_t* rbtree_find_min(rbnode_t* node)
{
    if (node == NULL)
    {
        return NULL;
    }

    rbnode_t* current = node;
    while (current->children[RBNODE_LEFT] != NULL)
    {
        current = current->children[RBNODE_LEFT];
    }
    return current;
}

rbnode_t* rbtree_find_max(rbnode_t* node)
{
    if (node == NULL)
    {
        return NULL;
    }

    rbnode_t* current = node;
    while (current->children[RBNODE_RIGHT] != NULL)
    {
        current = current->children[RBNODE_RIGHT];
    }
    return current;
}

void rbtree_swap(rbtree_t* tree, rbnode_t* a, rbnode_t* b)
{
    assert(tree != NULL);
    assert(a != NULL);
    assert(b != NULL);

    if (a == b)
    {
        return;
    }

    assert(a->parent == NULL || a->parent->children[RBNODE_LEFT] == a || a->parent->children[RBNODE_RIGHT] == a);
    assert(b->parent == NULL || b->parent->children[RBNODE_LEFT] == b || b->parent->children[RBNODE_RIGHT] == b);

    rbnode_color_t tempColor = a->color;
    a->color = b->color;
    b->color = tempColor;

    if (a->parent == b)
    {
        rbnode_t* temp = a;
        a = b;
        b = temp;
    }

    if (b->parent == a)
    {
        rbnode_t* aParent = a->parent;

        rbnode_direction_t sideB = (a->children[RBNODE_RIGHT] == b) ? RBNODE_RIGHT : RBNODE_LEFT;

        rbnode_t* otherChild = a->children[RBNODE_OPPOSITE(sideB)];
        rbnode_t* bLeft = b->children[RBNODE_LEFT];
        rbnode_t* bRight = b->children[RBNODE_RIGHT];

        b->parent = aParent;
        if (aParent != NULL)
        {
            if (aParent->children[RBNODE_LEFT] == a)
            {
                aParent->children[RBNODE_LEFT] = b;
            }
            else
            {
                aParent->children[RBNODE_RIGHT] = b;
            }
        }
        else
        {
            tree->root = b;
        }

        b->children[sideB] = a;
        a->parent = b;

        b->children[RBNODE_OPPOSITE(sideB)] = otherChild;
        if (otherChild != NULL)
        {
            otherChild->parent = b;
        }

        a->children[RBNODE_LEFT] = bLeft;
        if (bLeft != NULL)
        {
            bLeft->parent = a;
        }

        a->children[RBNODE_RIGHT] = bRight;
        if (bRight != NULL)
        {
            bRight->parent = a;
        }
    }
    else
    {
        rbnode_t* aParent = a->parent;
        rbnode_t* bParent = b->parent;
        rbnode_t* aLeft = a->children[RBNODE_LEFT];
        rbnode_t* aRight = a->children[RBNODE_RIGHT];
        rbnode_t* bLeft = b->children[RBNODE_LEFT];
        rbnode_t* bRight = b->children[RBNODE_RIGHT];

        a->parent = bParent;
        if (bParent != NULL)
        {
            if (bParent->children[RBNODE_LEFT] == b)
            {
                bParent->children[RBNODE_LEFT] = a;
            }
            else
            {
                bParent->children[RBNODE_RIGHT] = a;
            }
        }
        else
        {
            tree->root = a;
        }
        a->children[RBNODE_LEFT] = bLeft;
        if (bLeft != NULL)
        {
            bLeft->parent = a;
        }
        a->children[RBNODE_RIGHT] = bRight;
        if (bRight != NULL)
        {
            bRight->parent = a;
        }

        b->parent = aParent;
        if (aParent != NULL)
        {
            if (aParent->children[RBNODE_LEFT] == a)
            {
                aParent->children[RBNODE_LEFT] = b;
            }
            else
            {
                aParent->children[RBNODE_RIGHT] = b;
            }
        }
        else
        {
            tree->root = b;
        }
        b->children[RBNODE_LEFT] = aLeft;
        if (aLeft != NULL)
        {
            aLeft->parent = b;
        }
        b->children[RBNODE_RIGHT] = aRight;
        if (aRight != NULL)
        {
            aRight->parent = b;
        }
    }
}

static void rbtree_remove_sanitize(rbtree_t* tree, rbnode_t* node)
{
    assert(node->parent == NULL ||
        (node->parent->children[RBNODE_LEFT] != node && node->parent->children[RBNODE_RIGHT] != node));
    assert(node->children[RBNODE_LEFT] == NULL || (node->children[RBNODE_LEFT]->parent != node));
    assert(node->children[RBNODE_RIGHT] == NULL || (node->children[RBNODE_RIGHT]->parent != node));

    node->parent = NULL;
    node->children[RBNODE_LEFT] = NULL;
    node->children[RBNODE_RIGHT] = NULL;
    node->color = RBNODE_RED;

    tree->size--;
}

void rbtree_remove(rbtree_t* tree, rbnode_t* node)
{
    assert(tree != NULL);
    assert(node != NULL);

    if (tree->size == 0 || (node != tree->root && node->parent == NULL))
    {
        return;
    }

    assert(node == tree->root || node->parent->children[RBNODE_LEFT] == node ||
        node->parent->children[RBNODE_RIGHT] == node);

    if (node->children[RBNODE_LEFT] != NULL && node->children[RBNODE_RIGHT] != NULL)
    {
        rbnode_t* successor = rbtree_find_min(node->children[RBNODE_RIGHT]);
        assert(successor != NULL);
        assert(successor->children[RBNODE_LEFT] == NULL);
        rbtree_swap(tree, node, successor);
        rbtree_remove(tree, node);
        return;
    }

    rbnode_t* child = node->children[RBNODE_LEFT] != NULL ? node->children[RBNODE_LEFT] : node->children[RBNODE_RIGHT];
    assert(!(node->children[RBNODE_LEFT] != NULL && node->children[RBNODE_RIGHT] != NULL));

    if (node->color == RBNODE_RED)
    {
        assert(child == NULL);

        if (node->parent == NULL)
        {
            tree->root = NULL;
        }
        else
        {
            rbnode_direction_t fromParent = RBNODE_FROM_PARENT(node);
            assert(node->parent->children[fromParent] == node);
            node->parent->children[fromParent] = NULL;
        }

        rbtree_remove_sanitize(tree, node);
        return;
    }

    if (child != NULL)
    {
        assert(child->color == RBNODE_RED);
        assert(child->parent == node);

        if (node->parent == NULL)
        {
            tree->root = child;
        }
        else
        {
            node->parent->children[RBNODE_FROM_PARENT(node)] = child;
        }

        child->parent = node->parent;
        child->color = RBNODE_BLACK;

        rbtree_remove_sanitize(tree, node);
        return;
    }

    rbnode_t* parent = node->parent;

    if (parent == NULL)
    {
        tree->root = NULL;
        rbtree_remove_sanitize(tree, node);
        return;
    }

    rbnode_direction_t fromParent = RBNODE_FROM_PARENT(node);
    assert(parent->children[fromParent] == node);
    parent->children[fromParent] = NULL;

    rbtree_remove_sanitize(tree, node);

    // Do this very complex fixup stuff
    rbnode_t* curr = NULL;
    while (true)
    {
        rbnode_t* sibling = parent->children[RBNODE_OPPOSITE(fromParent)];
        assert(sibling != NULL);

        if (sibling->color == RBNODE_RED)
        {
            assert(parent->color == RBNODE_BLACK);
            rbtree_rotate(tree, parent, fromParent);
            parent->color = RBNODE_RED;
            sibling->color = RBNODE_BLACK;
            sibling = parent->children[RBNODE_OPPOSITE(fromParent)];
            assert(sibling != NULL);
            assert(sibling->color == RBNODE_BLACK);
        }

        rbnode_t* distantNephew = sibling->children[RBNODE_OPPOSITE(fromParent)];
        rbnode_t* closeNephew = sibling->children[fromParent];

        if ((distantNephew == NULL || distantNephew->color == RBNODE_BLACK) &&
            (closeNephew == NULL || closeNephew->color == RBNODE_BLACK))
        {
            sibling->color = RBNODE_RED;
            curr = parent;
            parent = curr->parent;

            if (parent == NULL)
            {
                break;
            }

            assert(parent->children[RBNODE_LEFT] == curr || parent->children[RBNODE_RIGHT] == curr);

            if (curr->color == RBNODE_RED)
            {
                curr->color = RBNODE_BLACK;
                break;
            }

            fromParent = RBNODE_FROM_PARENT(curr);
            continue;
        }

        if (distantNephew == NULL || distantNephew->color == RBNODE_BLACK)
        {
            assert(closeNephew != NULL);
            assert(closeNephew->color == RBNODE_RED);
            rbtree_rotate(tree, sibling, RBNODE_OPPOSITE(fromParent));
            sibling->color = RBNODE_RED;
            closeNephew->color = RBNODE_BLACK;

            distantNephew = sibling; // For clarity
            sibling = closeNephew;
        }

        assert(sibling->children[RBNODE_OPPOSITE(fromParent)] != NULL);
        assert(sibling->children[RBNODE_OPPOSITE(fromParent)]->color == RBNODE_RED);
        rbtree_rotate(tree, parent, fromParent);
        sibling->color = parent->color;
        parent->color = RBNODE_BLACK;
        if (sibling->children[RBNODE_OPPOSITE(fromParent)] != NULL)
        {
            sibling->children[RBNODE_OPPOSITE(fromParent)]->color = RBNODE_BLACK;
        }
        break;
    }

    if (tree->root != NULL)
    {
        tree->root->color = RBNODE_BLACK;
    }
}

bool rbtree_is_empty(const rbtree_t* tree)
{
    assert(tree != NULL);
    assert((tree->size == 0) == (tree->root == NULL));
    return tree->size == 0;
}

rbnode_t* rbtree_next(const rbnode_t* node)
{
    if (node == NULL)
    {
        return NULL;
    }

    if (node->children[RBNODE_RIGHT] != NULL)
    {
        return rbtree_find_min(node->children[RBNODE_RIGHT]);
    }

    rbnode_t* parent = node->parent;
    while (parent != NULL && node == parent->children[RBNODE_RIGHT])
    {
        node = parent;
        parent = parent->parent;
    }

    return parent;
}

rbnode_t* rbtree_prev(const rbnode_t* node)
{
    if (node == NULL)
    {
        return NULL;
    }

    if (node->children[RBNODE_LEFT] != NULL)
    {
        return rbtree_find_max(node->children[RBNODE_LEFT]);
    }

    rbnode_t* parent = node->parent;
    while (parent != NULL && node == parent->children[RBNODE_LEFT])
    {
        node = parent;
        parent = parent->parent;
    }

    return parent;
}