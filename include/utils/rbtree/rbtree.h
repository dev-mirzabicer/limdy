/**
 * @file rbtree.h
 * @brief Red-Black Tree implementation for memory pool management.
 *
 * This file contains the declarations for a Red-Black Tree data structure
 * used to efficiently manage and search memory pools in the Limdy project.
 *
 * @author Mirza Bicer
 * @date 2024-08-24
 */

#ifndef LIMDY_RBTREE_H
#define LIMDY_RBTREE_H

#include <stddef.h>
#include <stdbool.h>
#include "memory_pool.h"
#include "error_handler.h"
#include "limdy_alignment.h"

/**
 * @brief Enumeration of node colors in the Red-Black Tree.
 */
typedef enum
{
    LIMDY_RB_BLACK,
    LIMDY_RB_RED
} LimdyRBColor;

/**
 * @brief Structure representing a node in the Red-Black Tree.
 */
typedef struct LimdyRBNode
{
    LimdyMemoryPool *pool;
    LimdyRBColor color;
    struct LimdyRBNode *parent;
    struct LimdyRBNode *left;
    struct LimdyRBNode *right;
} LimdyRBNode;

/**
 * @brief Structure representing the Red-Black Tree.
 */
typedef struct
{
    LimdyRBNode *root;
    size_t size;
} LimdyRBTree;

/**
 * @brief Initialize a new Red-Black Tree.
 *
 * @param tree Pointer to the tree to initialize.
 * @return ErrorCode indicating success or failure.
 */
ErrorCode limdy_rbtree_init(LimdyRBTree *tree);

/**
 * @brief Insert a memory pool into the Red-Black Tree.
 *
 * @param tree Pointer to the tree.
 * @param pool Pointer to the memory pool to insert.
 * @return ErrorCode indicating success or failure.
 */
ErrorCode limdy_rbtree_insert(LimdyRBTree *tree, LimdyMemoryPool *pool);

/**
 * @brief Remove a memory pool from the Red-Black Tree.
 *
 * @param tree Pointer to the tree.
 * @param pool Pointer to the memory pool to remove.
 * @return ErrorCode indicating success or failure.
 */
ErrorCode limdy_rbtree_remove(LimdyRBTree *tree, LimdyMemoryPool *pool);

/**
 * @brief Find the best fit memory pool for a given size.
 *
 * @param tree Pointer to the tree.
 * @param size The size to find a best fit for.
 * @return Pointer to the best fit memory pool, or NULL if not found.
 */
LimdyMemoryPool *limdy_rbtree_find_best_fit(const LimdyRBTree *tree, size_t size);

/**
 * @brief Destroy the Red-Black Tree and free all associated resources.
 *
 * @param tree Pointer to the tree to destroy.
 */
void limdy_rbtree_destroy(LimdyRBTree *tree);

#endif // LIMDY_RBTREE_H