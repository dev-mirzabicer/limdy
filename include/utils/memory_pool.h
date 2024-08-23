/**
 * @file memory_pool.h
 * @brief Memory pool management system for efficient memory allocation.
 *
 * This file contains the declarations for the Limdy memory pool system,
 * which provides efficient memory allocation and deallocation for small
 * and large memory blocks. It uses a pool-based approach with a Red-Black Tree
 * for pool management and a slab allocator for small allocations to reduce
 * fragmentation and improve performance compared to standard malloc/free.
 *
 * @author Mirza Bicer
 * @date 2024-08-24
 * @version 1.2
 */

#ifndef LIMDY_UTILS_MEMORY_POOL_H
#define LIMDY_UTILS_MEMORY_POOL_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include "utils/error_handler.h"

#define ALIGN_SIZE(size, align) (((size) + (align) - 1) & ~((align) - 1))
#define MIN_BLOCK_SIZE (sizeof(struct MemoryBlock))
#define MEMORY_BLOCK_MAGIC 0xDEADBEEF

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
 * @brief Memory alignment used for allocations.
 */
#define LIMDY_MEMORY_ALIGNMENT 16

/**
 * @brief Number of slab sizes for the slab allocator.
 */
#define LIMDY_SLAB_SIZES 8

/**
 * @brief Minimum size for slab allocation.
 */
#define LIMDY_SLAB_MIN_SIZE 16

/**
 * @brief Maximum size for slab allocation.
 */
#define LIMDY_SLAB_MAX_SIZE 128

/**
 * @brief Default number of objects per slab.
 */
#define LIMDY_DEFAULT_SLAB_OBJECTS_PER_SLAB 64

/**
 * @brief Opaque structure representing a memory pool.
 */
typedef struct LimdyMemoryPool LimdyMemoryPool;

/**
 * @brief Configuration structure for memory pool system initialization.
 */
typedef struct
{
    size_t small_block_size;      /**< Size of small memory blocks */
    size_t small_pool_size;       /**< Size of small memory pools */
    size_t large_pool_size;       /**< Size of the large memory pool */
    size_t max_pools;             /**< Maximum number of memory pools */
    size_t slab_objects_per_slab; /**< Number of objects per slab */
} LimdyMemoryPoolConfig;

typedef struct
{
    void *slabs[LIMDY_SLAB_SIZES];
    size_t slab_sizes[LIMDY_SLAB_SIZES];
    size_t free_objects[LIMDY_SLAB_SIZES];
    pthread_mutex_t mutex;
} LimdySlabAllocator;

struct MemoryBlock
{
    uint32_t magic;
    size_t size;
    size_t in_use;
    struct MemoryBlock *next;
    struct MemoryBlock *prev;
    uintptr_t data[];
};

struct LimdyMemoryPool
{
    void *memory;
    struct MemoryBlock *free_list;
    size_t total_size;
    size_t used_size;
    pthread_mutex_t mutex;
    pthread_rwlock_t rwlock; // For read-heavy operations
};

static LimdyMemoryPool *small_pools[LIMDY_MAX_POOLS];
static LimdyMemoryPool *large_pool;
static size_t num_small_pools = 0;
static pthread_mutex_t global_mutex = PTHREAD_MUTEX_INITIALIZER;
static LimdyMemoryPoolConfig global_config;
static LimdyRBTree pool_rbtree;

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
 * This function allocates memory from the appropriate pool or slab based on the requested size.
 *
 * @param size The number of bytes to allocate.
 * @return A pointer to the allocated memory, or NULL if allocation fails.
 */
void *limdy_memory_pool_alloc(size_t size);

/**
 * @brief Free memory back to the pool.
 *
 * This function returns the previously allocated memory back to the appropriate pool or slab.
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
 * @brief Defragment a memory pool.
 *
 * This function attempts to reduce fragmentation in the specified memory pool.
 *
 * @param pool Pointer to the pool to defragment.
 * @return ErrorCode indicating success or failure.
 */
ErrorCode limdy_memory_pool_defragment(LimdyMemoryPool *pool);

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
 * @brief Resize an existing allocation in a specific pool.
 *
 * This function changes the size of the memory block pointed to by ptr in the specified pool.
 * If ptr is NULL, it behaves like limdy_memory_pool_alloc_from.
 * If size is zero, it behaves like limdy_memory_pool_free_to.
 *
 * @param pool Pointer to the pool to reallocate from.
 * @param ptr Pointer to the memory block to be resized.
 * @param new_size The new size for the memory block.
 * @return A pointer to the resized memory block, or NULL if reallocation fails.
 */
void *limdy_memory_pool_realloc_from(LimdyMemoryPool *pool, void *ptr, size_t new_size);

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
 * @brief Check if a pointer belongs to a specific memory pool.
 *
 * @param pool Pointer to the memory pool.
 * @param ptr Pointer to check.
 * @return true if the pointer belongs to the pool, false otherwise.
 */
bool limdy_memory_pool_contains(const LimdyMemoryPool *pool, const void *ptr);

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

/**
 * @brief Error code for memory corruption detection.
 */
#define LIMDY_MEMORY_POOL_ERROR_CORRUPTION (LIMDY_MEMORY_POOL_ERROR_BASE + 6)

#ifdef LIMDY_MEMORY_DEBUG
/**
 * @brief Check for memory leaks in the pool system.
 *
 * This function is only available when LIMDY_MEMORY_DEBUG is defined.
 * It prints information about any detected memory leaks.
 */
void limdy_memory_leak_check(void);
#endif

#endif // LIMDY_UTILS_MEMORY_POOL_H