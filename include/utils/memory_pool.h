/**
 * @file memory_pool.h
 * @brief Memory pool management system for efficient memory allocation.
 *
 * This file contains the declarations for the Limdy memory pool system,
 * which provides efficient memory allocation and deallocation for small
 * and large memory blocks. It uses a pool-based approach to reduce
 * fragmentation and improve performance compared to standard malloc/free.
 *
 * @author Mirza Bicer
 * @date 2024-08-22
 */

#ifndef LIMDY_UTILS_MEMORY_POOL_H
#define LIMDY_UTILS_MEMORY_POOL_H

#include <stddef.h>
#include <stdint.h>
#include "utils/error_handler.h"

/**
 * @brief Default size for small memory blocks.
 */
#define LIMDY_SMALL_BLOCK_SIZE 64

/**
 * @brief Default size for small memory pools (1MB).
 */
#define LIMDY_SMALL_POOL_SIZE (1024 * 1024)

/**
 * @brief Default size for large memory pools (10MB).
 */
#define LIMDY_LARGE_POOL_SIZE (10 * 1024 * 1024)

/**
 * @brief Maximum number of memory pools.
 */
#define LIMDY_MAX_POOLS 8

/**
 * @brief Opaque structure representing a memory pool.
 */
typedef struct LimdyMemoryPool LimdyMemoryPool;

/**
 * @brief Configuration structure for memory pool system initialization.
 */
typedef struct
{
    size_t small_block_size; /**< Size of small memory blocks */
    size_t small_pool_size;  /**< Size of small memory pools */
    size_t large_pool_size;  /**< Size of the large memory pool */
    size_t max_pools;        /**< Maximum number of memory pools */
} LimdyMemoryPoolConfig;

/**
 * @brief Initialize the memory pool system.
 *
 * This function must be called before any other memory pool functions.
 * It sets up the global memory pool system based on the provided configuration.
 *
 * @param config Pointer to the configuration structure.
 * @return ErrorCode indicating success or failure.
 */
ErrorCode limdy_memory_pool_init(const LimdyMemoryPoolConfig *config);

/**
 * @brief Clean up the memory pool system.
 *
 * This function should be called when the memory pool system is no longer needed.
 * It frees all allocated memory and destroys all pools.
 */
void limdy_memory_pool_cleanup(void);

/**
 * @brief Allocate memory from the pool.
 *
 * This function allocates memory from the appropriate pool based on the requested size.
 *
 * @param size The number of bytes to allocate.
 * @return A pointer to the allocated memory, or NULL if allocation fails.
 */
void *limdy_memory_pool_alloc(size_t size);

/**
 * @brief Free memory back to the pool.
 *
 * This function returns the previously allocated memory back to the appropriate pool.
 *
 * @param ptr Pointer to the memory to be freed.
 */
void limdy_memory_pool_free(void *ptr);

/**
 * @brief Resize an existing allocation.
 *
 * This function changes the size of the memory block pointed to by ptr.
 * If ptr is NULL, it behaves like limdy_memory_pool_alloc.
 * If size is zero, it behaves like limdy_memory_pool_free.
 *
 * @param ptr Pointer to the memory block to be resized.
 * @param new_size The new size for the memory block.
 * @return A pointer to the resized memory block, or NULL if reallocation fails.
 */
void *limdy_memory_pool_realloc(void *ptr, size_t new_size);

/**
 * @brief Get current memory usage statistics.
 *
 * This function provides information about the current memory usage of the pool system.
 *
 * @param total_allocated Pointer to store the total allocated memory size.
 * @param total_used Pointer to store the total used memory size.
 */
void limdy_memory_pool_get_stats(size_t *total_allocated, size_t *total_used);

/**
 * @brief Create a new memory pool.
 *
 * This function creates a new memory pool with the specified size.
 *
 * @param pool_size The size of the new pool to create.
 * @param new_pool Pointer to store the newly created pool.
 * @return ErrorCode indicating success or failure.
 */
ErrorCode limdy_memory_pool_create(size_t pool_size, LimdyMemoryPool **new_pool);

/**
 * @brief Destroy a memory pool.
 *
 * This function destroys a previously created memory pool and frees its resources.
 *
 * @param pool Pointer to the pool to be destroyed.
 */
void limdy_memory_pool_destroy(LimdyMemoryPool *pool);

/**
 * @brief Allocate memory from a specific pool.
 *
 * This function allocates memory from a specific pool rather than the global pool system.
 *
 * @param pool Pointer to the pool to allocate from.
 * @param size The number of bytes to allocate.
 * @return A pointer to the allocated memory, or NULL if allocation fails.
 */
void *limdy_memory_pool_alloc_from(LimdyMemoryPool *pool, size_t size);

/**
 * @brief Free memory back to a specific pool.
 *
 * This function returns the previously allocated memory back to a specific pool.
 *
 * @param pool Pointer to the pool to free memory to.
 * @param ptr Pointer to the memory to be freed.
 */
void limdy_memory_pool_free_to(LimdyMemoryPool *pool, void *ptr);

/**
 * @brief Base error code for memory pool errors.
 */
#define LIMDY_MEMORY_POOL_ERROR_BASE (ERROR_CUSTOM_BASE + 100)

/**
 * @brief Error code for memory pool initialization failure.
 */
#define LIMDY_MEMORY_POOL_ERROR_INIT_FAILED (LIMDY_MEMORY_POOL_ERROR_BASE + 1)

/**
 * @brief Error code for memory allocation failure.
 */
#define LIMDY_MEMORY_POOL_ERROR_ALLOC_FAILED (LIMDY_MEMORY_POOL_ERROR_BASE + 2)

/**
 * @brief Error code for invalid memory free operation.
 */
#define LIMDY_MEMORY_POOL_ERROR_INVALID_FREE (LIMDY_MEMORY_POOL_ERROR_BASE + 3)

/**
 * @brief Error code for when the maximum number of pools is reached.
 */
#define LIMDY_MEMORY_POOL_ERROR_POOL_FULL (LIMDY_MEMORY_POOL_ERROR_BASE + 4)

/**
 * @brief Error code for operations on an invalid pool.
 */
#define LIMDY_MEMORY_POOL_ERROR_INVALID_POOL (LIMDY_MEMORY_POOL_ERROR_BASE + 5)

#endif // LIMDY_UTILS_MEMORY_POOL_H