#include "memory_pool.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <assert.h>

#define ALIGN_SIZE(size, align) (((size) + (align) - 1) & ~((align) - 1))
#define MIN_BLOCK_SIZE (sizeof(struct MemoryBlock))
#define ALIGNMENT 16

struct MemoryBlock
{
    size_t size;
    int in_use;
    struct MemoryBlock *next;
    uintptr_t data[];
};

struct LimdyMemoryPool
{
    void *memory;
    struct MemoryBlock *free_list;
    size_t total_size;
    size_t used_size;
    pthread_mutex_t mutex;
};

static LimdyMemoryPool *small_pools[LIMDY_MAX_POOLS];
static LimdyMemoryPool *large_pool;
static size_t num_small_pools = 0;
static pthread_mutex_t global_mutex = PTHREAD_MUTEX_INITIALIZER;
static LimdyMemoryPoolConfig global_config;

static ErrorCode create_pool(size_t pool_size, LimdyMemoryPool **new_pool)
{
    *new_pool = (LimdyMemoryPool *)malloc(sizeof(LimdyMemoryPool));
    if (!*new_pool)
    {
        LOG_ERROR(LIMDY_MEMORY_POOL_ERROR_INIT_FAILED, "Failed to allocate memory for pool structure");
        return LIMDY_MEMORY_POOL_ERROR_INIT_FAILED;
    }

    (*new_pool)->memory = aligned_alloc(ALIGNMENT, pool_size);
    if (!(*new_pool)->memory)
    {
        free(*new_pool);
        LOG_ERROR(LIMDY_MEMORY_POOL_ERROR_INIT_FAILED, "Failed to allocate memory for pool");
        return LIMDY_MEMORY_POOL_ERROR_INIT_FAILED;
    }

    (*new_pool)->total_size = pool_size;
    (*new_pool)->used_size = 0;
    (*new_pool)->free_list = (struct MemoryBlock *)(*new_pool)->memory;
    (*new_pool)->free_list->size = pool_size - sizeof(struct MemoryBlock);
    (*new_pool)->free_list->in_use = 0;
    (*new_pool)->free_list->next = NULL;

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

    for (size_t i = 0; i < config->max_pools && i < LIMDY_MAX_POOLS; i++)
    {
        error = create_pool(config->small_pool_size, &small_pools[i]);
        if (error != ERROR_SUCCESS)
        {
            limdy_memory_pool_cleanup();
            return error;
        }
        num_small_pools++;
    }

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
}

static void *allocate_from_pool(LimdyMemoryPool *pool, size_t size)
{
    pthread_mutex_lock(&pool->mutex);

    struct MemoryBlock *block = pool->free_list;
    struct MemoryBlock *prev = NULL;

    while (block)
    {
        if (!block->in_use && block->size >= size)
        {
            if (block->size >= size + MIN_BLOCK_SIZE)
            {
                struct MemoryBlock *new_block = (struct MemoryBlock *)((char *)block + sizeof(struct MemoryBlock) + size);
                new_block->size = block->size - size - sizeof(struct MemoryBlock);
                new_block->in_use = 0;
                new_block->next = block->next;

                block->size = size;
                block->next = new_block;
            }

            block->in_use = 1;
            pool->used_size += block->size + sizeof(struct MemoryBlock);

            pthread_mutex_unlock(&pool->mutex);
            return block->data;
        }

        prev = block;
        block = block->next;
    }

    pthread_mutex_unlock(&pool->mutex);
    LOG_ERROR(LIMDY_MEMORY_POOL_ERROR_ALLOC_FAILED, "Failed to allocate memory from pool");
    return NULL;
}

void *limdy_memory_pool_alloc(size_t size)
{
    size = ALIGN_SIZE(size, ALIGNMENT);

    if (size <= global_config.small_block_size)
    {
        for (size_t i = 0; i < num_small_pools; i++)
        {
            void *ptr = allocate_from_pool(small_pools[i], size);
            if (ptr)
            {
                return ptr;
            }
        }
    }

    return allocate_from_pool(large_pool, size);
}

static LimdyMemoryPool *find_pool(void *ptr)
{
    for (size_t i = 0; i < num_small_pools; i++)
    {
        if (ptr >= small_pools[i]->memory && ptr < (char *)small_pools[i]->memory + small_pools[i]->total_size)
        {
            return small_pools[i];
        }
    }

    if (ptr >= large_pool->memory && ptr < (char *)large_pool->memory + large_pool->total_size)
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

    LimdyMemoryPool *pool = find_pool(ptr);
    if (!pool)
    {
        LOG_ERROR(LIMDY_MEMORY_POOL_ERROR_INVALID_FREE, "Attempt to free memory not allocated by pool");
        return;
    }

    pthread_mutex_lock(&pool->mutex);

    struct MemoryBlock *block = (struct MemoryBlock *)((char *)ptr - sizeof(struct MemoryBlock));
    assert(block->in_use);
    block->in_use = 0;
    pool->used_size -= block->size + sizeof(struct MemoryBlock);

    // Coalesce free blocks
    struct MemoryBlock *current = pool->free_list;
    while (current && current->next)
    {
        if (!current->in_use && !current->next->in_use)
        {
            current->size += current->next->size + sizeof(struct MemoryBlock);
            current->next = current->next->next;
        }
        else
        {
            current = current->next;
        }
    }

    pthread_mutex_unlock(&pool->mutex);
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

    struct MemoryBlock *block = (struct MemoryBlock *)((char *)ptr - sizeof(struct MemoryBlock));
    if (new_size <= block->size)
    {
        return ptr;
    }

    void *new_ptr = limdy_memory_pool_alloc(new_size);
    if (!new_ptr)
    {
        return NULL;
    }

    memcpy(new_ptr, ptr, block->size);
    limdy_memory_pool_free(ptr);

    return new_ptr;
}

void limdy_memory_pool_get_stats(size_t *total_allocated, size_t *total_used)
{
    *total_allocated = 0;
    *total_used = 0;

    for (size_t i = 0; i < num_small_pools; i++)
    {
        *total_allocated += small_pools[i]->total_size;
        *total_used += small_pools[i]->used_size;
    }

    *total_allocated += large_pool->total_size;
    *total_used += large_pool->used_size;
}

ErrorCode limdy_memory_pool_create(size_t pool_size, LimdyMemoryPool **new_pool)
{
    pthread_mutex_lock(&global_mutex);

    if (num_small_pools >= global_config.max_pools)
    {
        pthread_mutex_unlock(&global_mutex);
        LOG_ERROR(LIMDY_MEMORY_POOL_ERROR_POOL_FULL, "Maximum number of pools reached");
        return LIMDY_MEMORY_POOL_ERROR_POOL_FULL;
    }

    ErrorCode error = create_pool(pool_size, new_pool);
    if (error == ERROR_SUCCESS)
    {
        small_pools[num_small_pools++] = *new_pool;
    }

    pthread_mutex_unlock(&global_mutex);
    return error;
}

void limdy_memory_pool_destroy(LimdyMemoryPool *pool)
{
    if (!pool)
    {
        return;
    }

    pthread_mutex_lock(&global_mutex);

    for (size_t i = 0; i < num_small_pools; i++)
    {
        if (small_pools[i] == pool)
        {
            free(pool->memory);
            pthread_mutex_destroy(&pool->mutex);
            free(pool);
            small_pools[i] = small_pools[--num_small_pools];
            pthread_mutex_unlock(&global_mutex);
            return;
        }
    }

    pthread_mutex_unlock(&global_mutex);
    LOG_ERROR(LIMDY_MEMORY_POOL_ERROR_INVALID_POOL, "Attempt to destroy invalid pool");
}

void *limdy_memory_pool_alloc_from(LimdyMemoryPool *pool, size_t size)
{
    if (!pool)
    {
        LOG_ERROR(LIMDY_MEMORY_POOL_ERROR_INVALID_POOL, "Attempt to allocate from invalid pool");
        return NULL;
    }

    return allocate_from_pool(pool, ALIGN_SIZE(size, ALIGNMENT));
}

void limdy_memory_pool_free_to(LimdyMemoryPool *pool, void *ptr)
{
    if (!pool || !ptr)
    {
        return;
    }

    if (ptr < pool->memory || ptr >= (char *)pool->memory + pool->total_size)
    {
        LOG_ERROR(LIMDY_MEMORY_POOL_ERROR_INVALID_FREE, "Attempt to free memory to incorrect pool");
        return;
    }

    limdy_memory_pool_free(ptr);
}