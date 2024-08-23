/**
 * @file memory_pool.c
 * @brief Implementation of the memory pool management system.
 *
 * This file contains the implementation of the Limdy memory pool system,
 * providing efficient memory allocation and deallocation for small and large
 * memory blocks. It implements the interface defined in memory_pool.h.
 *
 * @author Mirza Bicer
 * @date 2024-08-22
 * @version 1.0
 */

#include "memory_pool.h"
#include "limdy_utils.h"
#include "rbtree.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static void error_fatal(ErrorCode code, const char *file, int line, const char *function, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    error_log(code, ERROR_LEVEL_FATAL, file, line, function, format, args);
    va_end(args);
    exit(EXIT_FAILURE);
}

static void verify_block_magic(struct MemoryBlock *block)
{
    if (block->magic != MEMORY_BLOCK_MAGIC)
    {
        error_fatal(LIMDY_MEMORY_POOL_ERROR_CORRUPTION, __FILE__, __LINE__, __func__, "Memory corruption detected: invalid magic number");
    }
}

static ErrorCode create_pool(size_t pool_size, LimdyMemoryPool **new_pool)
{
    *new_pool = (LimdyMemoryPool *)malloc(sizeof(LimdyMemoryPool));
    if (!*new_pool)
    {
        LOG_ERROR(LIMDY_MEMORY_POOL_ERROR_INIT_FAILED, "Failed to allocate memory for pool structure");
        return LIMDY_MEMORY_POOL_ERROR_INIT_FAILED;
    }

    (*new_pool)->memory = aligned_alloc(LIMDY_MEMORY_ALIGNMENT, pool_size);
    if (!(*new_pool)->memory)
    {
        free(*new_pool);
        LOG_ERROR(LIMDY_MEMORY_POOL_ERROR_INIT_FAILED, "Failed to allocate memory for pool");
        return LIMDY_MEMORY_POOL_ERROR_INIT_FAILED;
    }

    (*new_pool)->total_size = pool_size;
    (*new_pool)->used_size = 0;
    (*new_pool)->free_list = (struct MemoryBlock *)(*new_pool)->memory;
    (*new_pool)->free_list->magic = MEMORY_BLOCK_MAGIC;
    (*new_pool)->free_list->size = pool_size - sizeof(struct MemoryBlock);
    (*new_pool)->free_list->in_use = 0;
    (*new_pool)->free_list->next = NULL;
    (*new_pool)->free_list->prev = NULL;

    if (pthread_mutex_init(&(*new_pool)->mutex, NULL) != 0)
    {
        free((*new_pool)->memory);
        free(*new_pool);
        LOG_ERROR(LIMDY_MEMORY_POOL_ERROR_INIT_FAILED, "Failed to initialize mutex for pool");
        return LIMDY_MEMORY_POOL_ERROR_INIT_FAILED;
    }

    return ERROR_SUCCESS;
}

ErrorCode limdy_memory_pool_init(const LimdyMemoryPoolConfig *config)
{
    if (!config)
    {
        LOG_ERROR(ERROR_INVALID_ARGUMENT, "Invalid configuration");
        return ERROR_INVALID_ARGUMENT;
    }

    global_config = *config;

    ErrorCode error = create_pool(config->large_pool_size, &large_pool);
    if (error != ERROR_SUCCESS)
    {
        return error;
    }

    error = limdy_rbtree_init(&pool_rbtree);
    if (error != ERROR_SUCCESS)
    {
        limdy_memory_pool_cleanup();
        return error;
    }

    for (size_t i = 0; i < config->max_pools && i < LIMDY_MAX_POOLS; i++)
    {
        error = create_pool(config->small_pool_size, &small_pools[i]);
        if (error != ERROR_SUCCESS)
        {
            limdy_memory_pool_cleanup();
            return error;
        }
        error = limdy_rbtree_insert(&pool_rbtree, small_pools[i]);
        if (error != ERROR_SUCCESS)
        {
            limdy_memory_pool_cleanup();
            return error;
        }
        num_small_pools++;
    }

    // Initialize the slab allocator
    init_slab_allocator();

    return ERROR_SUCCESS;
}

void limdy_memory_pool_cleanup(void)
{
    for (size_t i = 0; i < num_small_pools; i++)
    {
        if (small_pools[i])
        {
            free(small_pools[i]->memory);
            pthread_mutex_destroy(&small_pools[i]->mutex);
            free(small_pools[i]);
            small_pools[i] = NULL;
        }
    }
    num_small_pools = 0;

    if (large_pool)
    {
        free(large_pool->memory);
        pthread_mutex_destroy(&large_pool->mutex);
        free(large_pool);
        large_pool = NULL;
    }

    limdy_rbtree_destroy(&pool_rbtree);

    // Cleanup the slab allocator
    for (int i = 0; i < LIMDY_SLAB_SIZES; i++)
    {
        if (slab_allocator.slabs[i])
        {
            free(slab_allocator.slabs[i]);
            slab_allocator.slabs[i] = NULL;
        }
    }
    pthread_mutex_destroy(&slab_allocator.mutex);
}

static void *allocate_from_pool(LimdyMemoryPool *pool, size_t size)
{
    MUTEX_LOCK(&pool->mutex);

    struct MemoryBlock *block = pool->free_list;
    struct MemoryBlock *prev = NULL;

    while (block)
    {
        verify_block_magic(block);
        if (!block->in_use && block->size >= size)
        {
            if (block->size >= size + MIN_BLOCK_SIZE)
            {
                struct MemoryBlock *new_block = (struct MemoryBlock *)((char *)block + sizeof(struct MemoryBlock) + size);
                new_block->magic = MEMORY_BLOCK_MAGIC;
                new_block->size = block->size - size - sizeof(struct MemoryBlock);
                new_block->in_use = 0;
                new_block->next = block->next;
                new_block->prev = block;
                if (block->next)
                {
                    block->next->prev = new_block;
                }
                block->next = new_block;
                block->size = size;
            }

            block->in_use = 1;
            pool->used_size += block->size + sizeof(struct MemoryBlock);

            MUTEX_UNLOCK(&pool->mutex);
            return block->data;
        }

        prev = block;
        block = block->next;
    }

    MUTEX_UNLOCK(&pool->mutex);
    LOG_ERROR(LIMDY_MEMORY_POOL_ERROR_ALLOC_FAILED, "Failed to allocate memory from pool");
    return NULL;
}

void *limdy_memory_pool_alloc(size_t size)
{
    size = ALIGN_SIZE(size, LIMDY_MEMORY_ALIGNMENT);

    // Try allocating from the slab allocator first
    if (size <= LIMDY_SLAB_MAX_SIZE)
    {
        void *ptr = slab_alloc(size);
        if (ptr)
        {
            return ptr;
        }
    }

    LimdyMemoryPool *suitable_pool = limdy_rbtree_find_best_fit(&pool_rbtree, size);
    if (suitable_pool)
    {
        void *ptr = allocate_from_pool(suitable_pool, size);
        if (ptr)
        {
            return ptr;
        }
    }

    return allocate_from_pool(large_pool, size);
}

static LimdyMemoryPool *find_pool(void *ptr)
{
    LimdyMemoryPool *pool = limdy_rbtree_find_best_fit(&pool_rbtree, (size_t)ptr);
    if (pool && limdy_memory_pool_contains(pool, ptr))
    {
        return pool;
    }

    if (limdy_memory_pool_contains(large_pool, ptr))
    {
        return large_pool;
    }

    return NULL;
}

void limdy_memory_pool_free(void *ptr)
{
    if (!ptr)
    {
        return;
    }

    // Check if the pointer belongs to the slab allocator
    if ((uintptr_t)ptr % LIMDY_MAX_ALIGN == 0)
    {
        // Assuming all slab-allocated memory is aligned to LIMDY_MAX_ALIGN
        for (int i = 0; i < LIMDY_SLAB_SIZES; i++)
        {
            if (slab_allocator.slabs[i] && ptr >= slab_allocator.slabs[i] &&
                ptr < (char *)slab_allocator.slabs[i] + slab_allocator.slab_sizes[i] * 64)
            {
                slab_free(ptr, slab_allocator.slab_sizes[i]);
                return;
            }
        }
    }

    LimdyMemoryPool *pool = find_pool(ptr);
    if (!pool)
    {
        LOG_ERROR(LIMDY_MEMORY_POOL_ERROR_INVALID_FREE, "Attempt to free memory not allocated by pool");
        return;
    }

    MUTEX_LOCK(&pool->mutex);

    struct MemoryBlock *block = (struct MemoryBlock *)((char *)ptr - sizeof(struct MemoryBlock));
    verify_block_magic(block);
    assert(block->in_use);
    block->in_use = 0;
    pool->used_size -= block->size + sizeof(struct MemoryBlock);

    // Coalesce with previous block if it's free
    if (block->prev && !block->prev->in_use)
    {
        block->prev->size += block->size + sizeof(struct MemoryBlock);
        block->prev->next = block->next;
        if (block->next)
        {
            block->next->prev = block->prev;
        }
        block = block->prev;
    }

    // Coalesce with next block if it's free
    if (block->next && !block->next->in_use)
    {
        block->size += block->next->size + sizeof(struct MemoryBlock);
        block->next = block->next->next;
        if (block->next)
        {
            block->next->prev = block;
        }
    }

    MUTEX_UNLOCK(&pool->mutex);
}

void *limdy_memory_pool_realloc(void *ptr, size_t new_size)
{
    if (!ptr)
    {
        return limdy_memory_pool_alloc(new_size);
    }

    if (new_size == 0)
    {
        limdy_memory_pool_free(ptr);
        return NULL;
    }

    LimdyMemoryPool *pool = find_pool(ptr);
    if (!pool)
    {
        LOG_ERROR(LIMDY_MEMORY_POOL_ERROR_INVALID_FREE, "Attempt to realloc memory not allocated by pool");
        return NULL;
    }

    MUTEX_LOCK(&pool->mutex);

    struct MemoryBlock *block = (struct MemoryBlock *)((char *)ptr - sizeof(struct MemoryBlock));
    verify_block_magic(block);
    if (!block->in_use)
    {
        MUTEX_UNLOCK(&pool->mutex);
        LOG_ERROR(LIMDY_MEMORY_POOL_ERROR_INVALID_FREE, "Attempt to reallocate freed memory");
        return NULL;
    }

    new_size = ALIGN_SIZE(new_size, LIMDY_MEMORY_ALIGNMENT);

    // Optimized reallocation: try extending the block if possible
    if (new_size > block->size && block->next && !block->next->in_use &&
        (block->size + sizeof(struct MemoryBlock) + block->next->size) >= new_size)
    {
        size_t total_size = block->size + sizeof(struct MemoryBlock) + block->next->size;
        block->size = new_size;
        if (total_size - new_size >= MIN_BLOCK_SIZE)
        {
            struct MemoryBlock *new_block = (struct MemoryBlock *)((char *)block + sizeof(struct MemoryBlock) + new_size);
            new_block->magic = MEMORY_BLOCK_MAGIC;
            new_block->size = total_size - new_size - sizeof(struct MemoryBlock);
            new_block->in_use = 0;
            new_block->next = block->next->next;
            new_block->prev = block;
            if (new_block->next)
            {
                new_block->next->prev = new_block;
            }
            block->next = new_block;
        }
        else
        {
            block->size = total_size;
            block->next = block->next->next;
            if (block->next)
            {
                block->next->prev = block;
            }
        }
        pool->used_size += new_size - block->size;
        MUTEX_UNLOCK(&pool->mutex);
        return ptr;
    }

    // If extending isn't possible, allocate a new block and copy data
    void *new_ptr = limdy_memory_pool_alloc(new_size);
    if (!new_ptr)
    {
        MUTEX_UNLOCK(&pool->mutex);
        LOG_ERROR(LIMDY_MEMORY_POOL_ERROR_ALLOC_FAILED, "Failed to allocate memory for realloc");
        return NULL;
    }

    memcpy(new_ptr, ptr, block->size);
    limdy_memory_pool_free(ptr);
    MUTEX_UNLOCK(&pool->mutex);

    return new_ptr;
}

ErrorCode limdy_memory_pool_defragment(LimdyMemoryPool *pool)
{
    if (!pool)
    {
        LOG_ERROR(LIMDY_MEMORY_POOL_ERROR_INVALID_POOL, "Attempt to defragment invalid pool");
        return LIMDY_MEMORY_POOL_ERROR_INVALID_POOL;
    }

    MUTEX_LOCK(&pool->mutex);

    struct MemoryBlock *current = pool->free_list;
    while (current && current->next)
    {
        if (!current->in_use && !current->next->in_use)
        {
            // Merge blocks
            current->size += current->next->size + sizeof(struct MemoryBlock);
            current->next = current->next->next;
            if (current->next)
            {
                current->next->prev = current;
            }
        }
        else
        {
            current = current->next;
        }
    }

    MUTEX_UNLOCK(&pool->mutex);
    return ERROR_SUCCESS;
}

static LimdySlabAllocator slab_allocator;

static void init_slab_allocator(void)
{
    pthread_mutex_init(&slab_allocator.mutex, NULL);
    for (int i = 0; i < LIMDY_SLAB_SIZES; i++)
    {
        slab_allocator.slab_sizes[i] = LIMDY_SLAB_MIN_SIZE << i;
        slab_allocator.slabs[i] = NULL;
        slab_allocator.free_objects[i] = 0;
    }
}

static void *slab_alloc(size_t size)
{
    if (size > LIMDY_SLAB_MAX_SIZE)
    {
        return NULL;
    }

    int slab_index = 0;
    while (slab_allocator.slab_sizes[slab_index] < size && slab_index < LIMDY_SLAB_SIZES)
    {
        slab_index++;
    }

    if (slab_index == LIMDY_SLAB_SIZES)
    {
        return NULL;
    }

    MUTEX_LOCK(&slab_allocator.mutex);

    if (!slab_allocator.free_objects[slab_index])
    {
        // Allocate new slab
        size_t slab_size = slab_allocator.slab_sizes[slab_index] * 64; // 64 objects per slab
        void *new_slab = aligned_alloc(LIMDY_MAX_ALIGN, slab_size);
        if (!new_slab)
        {
            MUTEX_UNLOCK(&slab_allocator.mutex);
            return NULL;
        }

        // Initialize free list
        for (size_t i = 0; i < 63; i++)
        {
            *(void **)((char *)new_slab + i * slab_allocator.slab_sizes[slab_index]) =
                (char *)new_slab + (i + 1) * slab_allocator.slab_sizes[slab_index];
        }
        *(void **)((char *)new_slab + 63 * slab_allocator.slab_sizes[slab_index]) = slab_allocator.slabs[slab_index];
        slab_allocator.slabs[slab_index] = new_slab;
        slab_allocator.free_objects[slab_index] = 64;
    }

    void *result = slab_allocator.slabs[slab_index];
    slab_allocator.slabs[slab_index] = *(void **)result;
    slab_allocator.free_objects[slab_index]--;

    MUTEX_UNLOCK(&slab_allocator.mutex);
    return result;
}

static void slab_free(void *ptr, size_t size)
{
    if (size > LIMDY_SLAB_MAX_SIZE)
    {
        return;
    }

    int slab_index = 0;
    while (slab_allocator.slab_sizes[slab_index] < size && slab_index < LIMDY_SLAB_SIZES)
    {
        slab_index++;
    }

    if (slab_index == LIMDY_SLAB_SIZES)
    {
        return;
    }

    MUTEX_LOCK(&slab_allocator.mutex);

    *(void **)ptr = slab_allocator.slabs[slab_index];
    slab_allocator.slabs[slab_index] = ptr;
    slab_allocator.free_objects[slab_index]++;

    MUTEX_UNLOCK(&slab_allocator.mutex);
}

void limdy_memory_pool_get_stats(size_t *total_allocated, size_t *total_used)
{
    MUTEX_LOCK(&global_mutex); // Ensure thread safety

    *total_allocated = 0;
    *total_used = 0;

    for (size_t i = 0; i < num_small_pools; i++)
    {
        *total_allocated += small_pools[i]->total_size;
        *total_used += small_pools[i]->used_size;
    }

    *total_allocated += large_pool->total_size;
    *total_used += large_pool->used_size;

    MUTEX_UNLOCK(&global_mutex);
}

ErrorCode limdy_memory_pool_create(size_t pool_size, LimdyMemoryPool **new_pool)
{
    MUTEX_LOCK(&global_mutex);
    if (num_small_pools >= global_config.max_pools)
    {
        MUTEX_UNLOCK(&global_mutex);
        LOG_ERROR(LIMDY_MEMORY_POOL_ERROR_POOL_FULL, "Maximum number of pools reached");
        return LIMDY_MEMORY_POOL_ERROR_POOL_FULL;
    }

    ErrorCode error = create_pool(pool_size, new_pool);
    if (error == ERROR_SUCCESS)
    {
        small_pools[num_small_pools] = *new_pool;
        error = limdy_rbtree_insert(&pool_rbtree, *new_pool);
        if (error == ERROR_SUCCESS)
        {
            num_small_pools++;
        }
        else
        {
            free((*new_pool)->memory);
            pthread_mutex_destroy(&(*new_pool)->mutex);
            free(*new_pool);
            *new_pool = NULL;
        }
    }

    MUTEX_UNLOCK(&global_mutex);
    return error;
}

void limdy_memory_pool_destroy(LimdyMemoryPool *pool)
{
    if (!pool)
    {
        return;
    }
    MUTEX_LOCK(&global_mutex);

    for (size_t i = 0; i < num_small_pools; i++)
    {
        if (small_pools[i] == pool)
        {
            free(pool->memory);
            pthread_mutex_destroy(&pool->mutex);
            limdy_rbtree_remove(&pool_rbtree, pool);
            free(pool);
            small_pools[i] = small_pools[--num_small_pools];
            MUTEX_UNLOCK(&global_mutex);
            return;
        }
    }

    // If pool not found in small_pools, check if it's the large_pool
    if (pool == large_pool)
    {
        free(pool->memory);
        pthread_mutex_destroy(&pool->mutex);
        free(pool);
        large_pool = NULL;
        MUTEX_UNLOCK(&global_mutex);
        return;
    }

    MUTEX_UNLOCK(&global_mutex);
    LOG_ERROR(LIMDY_MEMORY_POOL_ERROR_INVALID_POOL, "Attempt to destroy invalid pool");
}

void *limdy_memory_pool_alloc_from(LimdyMemoryPool *pool, size_t size)
{
    if (!pool)
    {
        LOG_ERROR(LIMDY_MEMORY_POOL_ERROR_INVALID_POOL, "Attempt to allocate from invalid pool");
        return NULL;
    }
    return allocate_from_pool(pool, ALIGN_SIZE(size, LIMDY_MEMORY_ALIGNMENT));
}

void *limdy_memory_pool_realloc_from(LimdyMemoryPool *pool, void *ptr, size_t new_size)
{
    if (!pool)
    {
        LOG_ERROR(LIMDY_MEMORY_POOL_ERROR_INVALID_POOL, "Attempt to reallocate from invalid pool");
        return NULL;
    }

    if (!ptr)
    {
        return limdy_memory_pool_alloc_from(pool, new_size);
    }

    if (new_size == 0)
    {
        limdy_memory_pool_free_to(pool, ptr);
        return NULL;
    }

    if (!limdy_memory_pool_contains(pool, ptr))
    {
        LOG_ERROR(LIMDY_MEMORY_POOL_ERROR_INVALID_FREE, "Attempt to reallocate memory from incorrect pool");
        return NULL;
    }

    MUTEX_LOCK(&pool->mutex);

    struct MemoryBlock *block = (struct MemoryBlock *)((char *)ptr - sizeof(struct MemoryBlock));
    verify_block_magic(block);
    if (!block->in_use)
    {
        MUTEX_UNLOCK(&pool->mutex);
        LOG_ERROR(LIMDY_MEMORY_POOL_ERROR_INVALID_FREE, "Attempt to reallocate freed memory");
        return NULL;
    }

    new_size = ALIGN_SIZE(new_size, LIMDY_MEMORY_ALIGNMENT);

    // Optimized reallocation: try extending the block if possible
    if (new_size > block->size && block->next && !block->next->in_use &&
        (block->size + sizeof(struct MemoryBlock) + block->next->size) >= new_size)
    {
        size_t total_size = block->size + sizeof(struct MemoryBlock) + block->next->size;
        block->size = new_size;
        if (total_size - new_size >= MIN_BLOCK_SIZE)
        {
            struct MemoryBlock *new_block = (struct MemoryBlock *)((char *)block + sizeof(struct MemoryBlock) + new_size);
            new_block->magic = MEMORY_BLOCK_MAGIC;
            new_block->size = total_size - new_size - sizeof(struct MemoryBlock);
            new_block->in_use = 0;
            new_block->next = block->next->next;
            new_block->prev = block;
            if (new_block->next)
            {
                new_block->next->prev = new_block;
            }
            block->next = new_block;
        }
        else
        {
            block->size = total_size;
            block->next = block->next->next;
            if (block->next)
            {
                block->next->prev = block;
            }
        }
        pool->used_size += new_size - block->size;
        MUTEX_UNLOCK(&pool->mutex);
        return ptr;
    }

    // If extending isn't possible, allocate a new block and copy data
    void *new_ptr = limdy_memory_pool_alloc_from(pool, new_size);
    if (!new_ptr)
    {
        MUTEX_UNLOCK(&pool->mutex);
        return NULL;
    }

    memcpy(new_ptr, ptr, block->size);
    limdy_memory_pool_free_to(pool, ptr);
    MUTEX_UNLOCK(&pool->mutex);

    return new_ptr;
}

void limdy_memory_pool_free_to(LimdyMemoryPool *pool, void *ptr)
{
    if (!pool || !ptr)
    {
        return;
    }

    if (!limdy_memory_pool_contains(pool, ptr))
    {
        LOG_ERROR(LIMDY_MEMORY_POOL_ERROR_INVALID_FREE, "Attempt to free memory to incorrect pool");
        return;
    }

    MUTEX_LOCK(&pool->mutex);

    struct MemoryBlock *block = (struct MemoryBlock *)((char *)ptr - sizeof(struct MemoryBlock));
    verify_block_magic(block);
    assert(block->in_use);
    block->in_use = 0;
    pool->used_size -= block->size + sizeof(struct MemoryBlock);

    // Coalesce with previous block if it's free
    if (block->prev && !block->prev->in_use)
    {
        block->prev->size += block->size + sizeof(struct MemoryBlock);
        block->prev->next = block->next;
        if (block->next)
        {
            block->next->prev = block->prev;
        }
        block = block->prev;
    }

    // Coalesce with next block if it's free
    if (block->next && !block->next->in_use)
    {
        block->size += block->next->size + sizeof(struct MemoryBlock);
        block->next = block->next->next;
        if (block->next)
        {
            block->next->prev = block;
        }
    }

    MUTEX_UNLOCK(&pool->mutex);
}

bool limdy_memory_pool_contains(const LimdyMemoryPool *pool, const void *ptr)
{
    if (!pool || !ptr)
    {
        return false;
    }
    return (ptr >= pool->memory && ptr < (char *)pool->memory + pool->total_size);
}

#ifdef LIMDY_MEMORY_DEBUG
static pthread_mutex_t debug_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct MemoryAllocation
{
    void *ptr;
    size_t size;
    const char *file;
    int line;
    struct MemoryAllocation *next;
} *debug_allocations = NULL;

void *limdy_memory_pool_alloc_debug(size_t size, const char *file, int line)
{
    void *ptr = limdy_memory_pool_alloc(size);
    if (ptr)
    {
        MUTEX_LOCK(&debug_mutex);
        struct MemoryAllocation *alloc = malloc(sizeof(struct MemoryAllocation));
        alloc->ptr = ptr;
        alloc->size = size;
        alloc->file = file;
        alloc->line = line;
        alloc->next = debug_allocations;
        debug_allocations = alloc;
        MUTEX_UNLOCK(&debug_mutex);
    }
    return ptr;
}

void limdy_memory_pool_free_debug(void *ptr, const char *file, int line)
{
    limdy_memory_pool_free(ptr);
    MUTEX_LOCK(&debug_mutex);
    struct MemoryAllocation **prev = &debug_allocations;
    struct MemoryAllocation *current = debug_allocations;
    while (current)
    {
        if (current->ptr == ptr)
        {
            *prev = current->next;
            free(current);
            break;
        }
        prev = ¤t->next;
        current = current->next;
    }
    MUTEX_UNLOCK(&debug_mutex);
}

void limdy_memory_leak_check(void)
{
    MUTEX_LOCK(&debug_mutex);
    struct MemoryAllocation *current = debug_allocations;
    while (current)
    {
        fprintf(stderr, "Memory leak detected: %zu bytes allocated at %s:%d\n",
                current->size, current->file, current->line);
        current = current->next;
    }
    MUTEX_UNLOCK(&debug_mutex);
}

#define limdy_memory_pool_alloc(size) limdy_memory_pool_alloc_debug(size, __FILE__, __LINE__)
#define limdy_memory_pool_free(ptr) limdy_memory_pool_free_debug(ptr, __FILE__, __LINE__)
#endif