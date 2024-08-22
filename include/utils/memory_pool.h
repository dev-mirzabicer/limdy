#ifndef LIMDY_UTILS_MEMORY_POOL_H
#define LIMDY_UTILS_MEMORY_POOL_H

#include <stddef.h>
#include <stdint.h>
#include "utils/error_handler.h"

#define LIMDY_SMALL_BLOCK_SIZE 64
#define LIMDY_SMALL_POOL_SIZE (1024 * 1024)      // 1MB
#define LIMDY_LARGE_POOL_SIZE (10 * 1024 * 1024) // 10MB
#define LIMDY_MAX_POOLS 8

typedef struct LimdyMemoryPool LimdyMemoryPool;

typedef struct
{
    size_t small_block_size;
    size_t small_pool_size;
    size_t large_pool_size;
    size_t max_pools;
} LimdyMemoryPoolConfig;

// Initialize the memory pool system
ErrorCode limdy_memory_pool_init(const LimdyMemoryPoolConfig *config);

// Cleanup the memory pool system
void limdy_memory_pool_cleanup(void);

// Allocate memory from the pool
void *limdy_memory_pool_alloc(size_t size);

// Free memory back to the pool
void limdy_memory_pool_free(void *ptr);

// Resize an existing allocation
void *limdy_memory_pool_realloc(void *ptr, size_t new_size);

// Get current memory usage statistics
void limdy_memory_pool_get_stats(size_t *total_allocated, size_t *total_used);

// Create a new memory pool
ErrorCode limdy_memory_pool_create(size_t pool_size, LimdyMemoryPool **new_pool);

// Destroy a memory pool
void limdy_memory_pool_destroy(LimdyMemoryPool *pool);

// Allocate from a specific pool
void *limdy_memory_pool_alloc_from(LimdyMemoryPool *pool, size_t size);

// Free to a specific pool
void limdy_memory_pool_free_to(LimdyMemoryPool *pool, void *ptr);

// Memory pool error codes
#define LIMDY_MEMORY_POOL_ERROR_BASE (ERROR_CUSTOM_BASE + 100)
#define LIMDY_MEMORY_POOL_ERROR_INIT_FAILED (LIMDY_MEMORY_POOL_ERROR_BASE + 1)
#define LIMDY_MEMORY_POOL_ERROR_ALLOC_FAILED (LIMDY_MEMORY_POOL_ERROR_BASE + 2)
#define LIMDY_MEMORY_POOL_ERROR_INVALID_FREE (LIMDY_MEMORY_POOL_ERROR_BASE + 3)
#define LIMDY_MEMORY_POOL_ERROR_POOL_FULL (LIMDY_MEMORY_POOL_ERROR_BASE + 4)
#define LIMDY_MEMORY_POOL_ERROR_INVALID_POOL (LIMDY_MEMORY_POOL_ERROR_BASE + 5)

#endif // LIMDY_UTILS_MEMORY_POOL_H