#include <_internal/NULL.h>
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
    node->color = RBNODE_RED;
    node->parent = parent;

    if (parent == NULL)
    {
        tree->root = node;
        return;
    }

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
            return ;
        }

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
}

rbnode_t* rbtree_find_min(rbnode_t* node)
{
    assert(node != NULL);

    rbnode_t* current = node;
    while (current->children[RBNODE_LEFT] != NULL)
    {
        current = current->children[RBNODE_LEFT];
    }
    return current;
}

rbnode_t* rbtree_find_max(rbnode_t* node)
{
    assert(node != NULL);

    rbnode_t* current = node;
    while (current->children[RBNODE_RIGHT] != NULL)
    {
        current = current->children[RBNODE_RIGHT];
    }
    return current;
}

void rbtree_swap(rbnode_t* a, rbnode_t* b)
{
    assert(a != NULL);
    assert(b != NULL);

    rbnode_t* tempParent = a->parent;
    a->parent = b->parent;
    b->parent = tempParent;

    rbnode_t* tempLeft = a->children[RBNODE_LEFT];
    rbnode_t* tempRight = a->children[RBNODE_RIGHT];
    a->children[RBNODE_LEFT] = b->children[RBNODE_LEFT];
    a->children[RBNODE_RIGHT] = b->children[RBNODE_RIGHT];
    b->children[RBNODE_LEFT] = tempLeft;
    b->children[RBNODE_RIGHT] = tempRight;

    if (a->children[RBNODE_LEFT] != NULL)
    {
        a->children[RBNODE_LEFT]->parent = a;
    }
    if (a->children[RBNODE_RIGHT] != NULL)
    {
        a->children[RBNODE_RIGHT]->parent = a;
    }
    if (b->children[RBNODE_LEFT] != NULL)
    {
        b->children[RBNODE_LEFT]->parent = b;
    }
    if (b->children[RBNODE_RIGHT] != NULL)
    {
        b->children[RBNODE_RIGHT]->parent = b;
    }

    rbnode_color_t tempColor = a->color;
    a->color = b->color;
    b->color = tempColor;
}

void rbtree_remove(rbtree_t* tree, rbnode_t* node)
{
    assert(tree != NULL);
    assert(node != NULL);
    
    if (tree->size == 0 || (node != tree->root && node->parent == NULL))
    {
        return;
    }

    // There are a bunch of cases for this one, just check the wikipedia pages "Removal" section.

    uint8_t childAmount = 0;
    if (node->children[RBNODE_LEFT] != NULL)
    {
        childAmount++;
    }
    if (node->children[RBNODE_RIGHT] != NULL)
    {
        childAmount++;
    }

    if (childAmount == 2)
    {
        rbnode_t* successor = rbtree_find_min(node->children[RBNODE_RIGHT]);
        rbtree_swap(node, successor);
        rbtree_remove(tree, successor);

        tree->size--;
        return;
    }

    if (childAmount == 1)
    {
        rbnode_t* child = node->children[RBNODE_LEFT] != NULL ? node->children[RBNODE_LEFT] : node->children[RBNODE_RIGHT];

        assert(child != NULL);
        assert(child->color == RBNODE_RED);
        assert(node->color == RBNODE_BLACK);
        
        if (node->parent == NULL)
        {
            tree->root = child;
            child->parent = NULL;
        }
        else
        {
            rbnode_direction_t fromParent = RBNODE_FROM_PARENT(node);
            node->parent->children[fromParent] = child;
            child->parent = node->parent;
        }

        child->color = RBNODE_BLACK;
        tree->size--;
        return;
    }

    if (childAmount == 0 && node->parent == NULL)
    {
        tree->root = NULL;
        tree->size--;
        assert(tree->size == 0);
        return;
    }

    if (childAmount == 0 && node->color == RBNODE_RED)
    {
        rbnode_direction_t fromParent = RBNODE_FROM_PARENT(node);
        node->parent->children[fromParent] = NULL;
        tree->size--;
        return;
    }

    assert(childAmount == 0 && node->color == RBNODE_BLACK);
    assert(node->parent != NULL);

    rbnode_t* parent = node->parent;
    rbnode_direction_t fromParent = RBNODE_FROM_PARENT(node);

    rbnode_t* sibling = NULL;
    rbnode_t* distantNephew = NULL;
    rbnode_t* closeNephew  = NULL;
    while (true)
    {
        sibling = parent->children[RBNODE_OPPOSITE(fromParent)];
        distantNephew = sibling->children[RBNODE_OPPOSITE(fromParent)];
        closeNephew = sibling->children[fromParent];

        if (sibling->color == RBNODE_RED)
        {
            rbtree_rotate(tree, parent, fromParent);
            parent->color = RBNODE_RED;
            sibling->color = RBNODE_BLACK;
            sibling = closeNephew;

            distantNephew = sibling->children[RBNODE_OPPOSITE(fromParent)];
            if (distantNephew != NULL && distantNephew->color == RBNODE_RED)
            {
                goto case_6;
            }

            closeNephew = sibling->children[fromParent];
            if (closeNephew != NULL && closeNephew->color == RBNODE_RED)
            {
                goto case_5;
            }

            sibling->color = RBNODE_RED;
            parent->color = RBNODE_BLACK;
            tree->size--;
            return;
        }

        if (distantNephew != NULL && distantNephew->color == RBNODE_RED)
        {
            goto case_6;
        }

        if (closeNephew != NULL && closeNephew->color == RBNODE_RED)
        {
            goto case_5;
        }

        if (parent == NULL)
        {
            tree->size--;
            return;
        }

        if (parent->color == RBNODE_RED)
        {
            sibling->color = RBNODE_RED;
            parent->color = RBNODE_BLACK;
            tree->size--;
            return;
        }

        sibling->color = RBNODE_RED;
        node = parent;
        parent = node->parent;
        if (parent == NULL)
        {
            break;
        }

        fromParent = RBNODE_FROM_PARENT(node);
    }

case_5:

    rbtree_rotate(tree, sibling, RBNODE_OPPOSITE(fromParent));
    sibling->color = RBNODE_RED;
    closeNephew->color = RBNODE_BLACK;
    distantNephew = sibling;
    sibling = closeNephew;

case_6:

    rbtree_rotate(tree, parent, fromParent);
    sibling->color = parent->color;
    parent->color = RBNODE_BLACK;
    distantNephew->color = RBNODE_BLACK;

    tree->size--;
}