/**
 * @file rbtree.c
 * @brief Implementation of Red-Black Tree for memory pool management.
 *
 * This file contains the implementation of the Red-Black Tree data structure
 * used to efficiently manage and search memory pools in the Limdy project.
 *
 * @author Mirza Bicer
 * @date 2024-08-24
 */

#include "rbtree.h"
#include "limdy_utils.h"
#include <stdlib.h>

static LimdyRBNode *create_node(LimdyMemoryPool *pool)
{
    LimdyRBNode *node = (LimdyRBNode *)limdy_memory_pool_alloc(sizeof(LimdyRBNode));
    if (node == NULL)
    {
        return NULL;
    }
    node->pool = pool;
    node->color = LIMDY_RB_RED;
    node->parent = node->left = node->right = NULL;
    return node;
}

static void left_rotate(LimdyRBTree *tree, LimdyRBNode *x)
{
    LimdyRBNode *y = x->right;
    x->right = y->left;
    if (y->left != NULL)
    {
        y->left->parent = x;
    }
    y->parent = x->parent;
    if (x->parent == NULL)
    {
        tree->root = y;
    }
    else if (x == x->parent->left)
    {
        x->parent->left = y;
    }
    else
    {
        x->parent->right = y;
    }
    y->left = x;
    x->parent = y;
}

static void right_rotate(LimdyRBTree *tree, LimdyRBNode *y)
{
    LimdyRBNode *x = y->left;
    y->left = x->right;
    if (x->right != NULL)
    {
        x->right->parent = y;
    }
    x->parent = y->parent;
    if (y->parent == NULL)
    {
        tree->root = x;
    }
    else if (y == y->parent->right)
    {
        y->parent->right = x;
    }
    else
    {
        y->parent->left = x;
    }
    x->right = y;
    y->parent = x;
}

static void insert_fixup(LimdyRBTree *tree, LimdyRBNode *node)
{
    while (node != tree->root && node->parent->color == LIMDY_RB_RED)
    {
        if (node->parent == node->parent->parent->left)
        {
            LimdyRBNode *uncle = node->parent->parent->right;
            if (uncle != NULL && uncle->color == LIMDY_RB_RED)
            {
                node->parent->color = LIMDY_RB_BLACK;
                uncle->color = LIMDY_RB_BLACK;
                node->parent->parent->color = LIMDY_RB_RED;
                node = node->parent->parent;
            }
            else
            {
                if (node == node->parent->right)
                {
                    node = node->parent;
                    left_rotate(tree, node);
                }
                node->parent->color = LIMDY_RB_BLACK;
                node->parent->parent->color = LIMDY_RB_RED;
                right_rotate(tree, node->parent->parent);
            }
        }
        else
        {
            LimdyRBNode *uncle = node->parent->parent->left;
            if (uncle != NULL && uncle->color == LIMDY_RB_RED)
            {
                node->parent->color = LIMDY_RB_BLACK;
                uncle->color = LIMDY_RB_BLACK;
                node->parent->parent->color = LIMDY_RB_RED;
                node = node->parent->parent;
            }
            else
            {
                if (node == node->parent->left)
                {
                    node = node->parent;
                    right_rotate(tree, node);
                }
                node->parent->color = LIMDY_RB_BLACK;
                node->parent->parent->color = LIMDY_RB_RED;
                left_rotate(tree, node->parent->parent);
            }
        }
    }
    tree->root->color = LIMDY_RB_BLACK;
}

static void transplant(LimdyRBTree *tree, LimdyRBNode *u, LimdyRBNode *v)
{
    if (u->parent == NULL)
    {
        tree->root = v;
    }
    else if (u == u->parent->left)
    {
        u->parent->left = v;
    }
    else
    {
        u->parent->right = v;
    }
    if (v != NULL)
    {
        v->parent = u->parent;
    }
}

static LimdyRBNode *tree_minimum(LimdyRBNode *node)
{
    while (node->left != NULL)
    {
        node = node->left;
    }
    return node;
}

static void delete_fixup(LimdyRBTree *tree, LimdyRBNode *x, LimdyRBNode *parent)
{
    while (x != tree->root && (x == NULL || x->color == LIMDY_RB_BLACK))
    {
        if (x == parent->left)
        {
            LimdyRBNode *w = parent->right;
            if (w->color == LIMDY_RB_RED)
            {
                w->color = LIMDY_RB_BLACK;
                parent->color = LIMDY_RB_RED;
                left_rotate(tree, parent);
                w = parent->right;
            }
            if ((w->left == NULL || w->left->color == LIMDY_RB_BLACK) &&
                (w->right == NULL || w->right->color == LIMDY_RB_BLACK))
            {
                w->color = LIMDY_RB_RED;
                x = parent;
                parent = x->parent;
            }
            else
            {
                if (w->right == NULL || w->right->color == LIMDY_RB_BLACK)
                {
                    if (w->left != NULL)
                    {
                        w->left->color = LIMDY_RB_BLACK;
                    }
                    w->color = LIMDY_RB_RED;
                    right_rotate(tree, w);
                    w = parent->right;
                }
                w->color = parent->color;
                parent->color = LIMDY_RB_BLACK;
                if (w->right != NULL)
                {
                    w->right->color = LIMDY_RB_BLACK;
                }
                left_rotate(tree, parent);
                x = tree->root;
                break;
            }
        }
        else
        {
            LimdyRBNode *w = parent->left;
            if (w->color == LIMDY_RB_RED)
            {
                w->color = LIMDY_RB_BLACK;
                parent->color = LIMDY_RB_RED;
                right_rotate(tree, parent);
                w = parent->left;
            }
            if ((w->right == NULL || w->right->color == LIMDY_RB_BLACK) &&
                (w->left == NULL || w->left->color == LIMDY_RB_BLACK))
            {
                w->color = LIMDY_RB_RED;
                x = parent;
                parent = x->parent;
            }
            else
            {
                if (w->left == NULL || w->left->color == LIMDY_RB_BLACK)
                {
                    if (w->right != NULL)
                    {
                        w->right->color = LIMDY_RB_BLACK;
                    }
                    w->color = LIMDY_RB_RED;
                    left_rotate(tree, w);
                    w = parent->left;
                }
                w->color = parent->color;
                parent->color = LIMDY_RB_BLACK;
                if (w->left != NULL)
                {
                    w->left->color = LIMDY_RB_BLACK;
                }
                right_rotate(tree, parent);
                x = tree->root;
                break;
            }
        }
    }
    if (x != NULL)
    {
        x->color = LIMDY_RB_BLACK;
    }
}

ErrorCode limdy_rbtree_init(LimdyRBTree *tree)
{
    CHECK_NULL(tree, ERROR_INVALID_ARGUMENT);
    tree->root = NULL;
    tree->size = 0;
    return ERROR_SUCCESS;
}

ErrorCode limdy_rbtree_insert(LimdyRBTree *tree, LimdyMemoryPool *pool)
{
    CHECK_NULL(tree, ERROR_INVALID_ARGUMENT);
    CHECK_NULL(pool, ERROR_INVALID_ARGUMENT);

    LimdyRBNode *node = create_node(pool);
    if (node == NULL)
    {
        LOG_ERROR(ERROR_MEMORY_ALLOCATION, "Failed to allocate memory for new RB-tree node");
        return ERROR_MEMORY_ALLOCATION;
    }

    LimdyRBNode *y = NULL;
    LimdyRBNode *x = tree->root;

    while (x != NULL)
    {
        y = x;
        if (pool->total_size < x->pool->total_size)
        {
            x = x->left;
        }
        else
        {
            x = x->right;
        }
    }

    node->parent = y;
    if (y == NULL)
    {
        tree->root = node;
    }
    else if (pool->total_size < y->pool->total_size)
    {
        y->left = node;
    }
    else
    {
        y->right = node;
    }

    insert_fixup(tree, node);
    tree->size++;

    return ERROR_SUCCESS;
}

ErrorCode limdy_rbtree_remove(LimdyRBTree *tree, LimdyMemoryPool *pool)
{
    CHECK_NULL(tree, ERROR_INVALID_ARGUMENT);
    CHECK_NULL(pool, ERROR_INVALID_ARGUMENT);

    LimdyRBNode *z = tree->root;
    while (z != NULL)
    {
        if (pool->total_size == z->pool->total_size)
        {
            break;
        }
        if (pool->total_size < z->pool->total_size)
        {
            z = z->left;
        }
        else
        {
            z = z->right;
        }
    }

    if (z == NULL)
    {
        LOG_ERROR(LIMDY_MEMORY_POOL_ERROR_INVALID_POOL, "Attempt to remove non-existent pool from RB-tree");
        return LIMDY_MEMORY_POOL_ERROR_INVALID_POOL;
    }

    LimdyRBNode *y = z;
    LimdyRBNode *x;
    LimdyRBColor y_original_color = y->color;

    if (z->left == NULL)
    {
        x = z->right;
        transplant(tree, z, z->right);
    }
    else if (z->right == NULL)
    {
        x = z->left;
        transplant(tree, z, z->left);
    }
    else
    {
        y = tree_minimum(z->right);
        y_original_color = y->color;
        x = y->right;
        if (y->parent == z)
        {
            if (x != NULL)
            {
                x->parent = y;
            }
        }
        else
        {
            transplant(tree, y, y->right);
            y->right = z->right;
            y->right->parent = y;
        }
        transplant(tree, z, y);
        y->left = z->left;
        y->left->parent = y;
        y->color = z->color;
    }

    if (y_original_color == LIMDY_RB_BLACK)
    {
        delete_fixup(tree, x, (x == NULL) ? y->parent : x->parent);
    }

    limdy_memory_pool_free(z);
    tree->size--;

    return ERROR_SUCCESS;
}

LimdyMemoryPool *limdy_rbtree_find_best_fit(const LimdyRBTree *tree, size_t size)
{
    CHECK_NULL(tree, NULL);

    LimdyRBNode *current = tree->root;
    LimdyMemoryPool *best_fit = NULL;

    while (current != NULL)
    {
        if (current->pool->total_size >= size)
        {
            best_fit = current->pool;
            current = current->left;
        }
        else
        {
            current = current->right;
        }
    }

    return best_fit;
}

static void destroy_node(LimdyRBNode *node)
{
    if (node != NULL)
    {
        destroy_node(node->left);
        destroy_node(node->right);
        limdy_memory_pool_free(node);
    }
}

void limdy_rbtree_destroy(LimdyRBTree *tree)
{
    if (tree != NULL)
    {
        destroy_node(tree->root);
        tree->root = NULL;
        tree->size = 0;
    }
}

#ifdef LIMDY_MEMORY_DEBUG
void limdy_rbtree_validate(const LimdyRBTree *tree)
{
    if (tree == NULL || tree->root == NULL)
    {
        return;
    }

    // Check if root is black
    assert(tree->root->color == LIMDY_RB_BLACK);

    // Check if every path from root to leaves has the same number of black nodes
    int black_count = 0;
    const LimdyRBNode *node = tree->root;
    while (node != NULL)
    {
        if (node->color == LIMDY_RB_BLACK)
        {
            black_count++;
        }
        node = node->left;
    }

    int path_black_count = 0;
    bool valid = true;

    void validate_node(const LimdyRBNode *node)
    {
        if (node == NULL)
        {
            if (path_black_count != black_count)
            {
                valid = false;
            }
            return;
        }

        // Check if red node has only black children
        if (node->color == LIMDY_RB_RED)
        {
            if ((node->left != NULL && node->left->color == LIMDY_RB_RED) ||
                (node->right != NULL && node->right->color == LIMDY_RB_RED))
            {
                valid = false;
            }
        }

        if (node->color == LIMDY_RB_BLACK)
        {
            path_black_count++;
        }

        validate_node(node->left);
        validate_node(node->right);

        if (node->color == LIMDY_RB_BLACK)
        {
            path_black_count--;
        }
    }

    validate_node(tree->root);

    assert(valid);
}
#endif // LIMDY_MEMORY_DEBUG