#include "linguistic_element.h"
#include <string.h>
#include <pthread.h>

#define FNV_PRIME 1099511628211ULL
#define FNV_OFFSET 14695981039346656037ULL
#define LOAD_FACTOR_THRESHOLD 0.75

static pthread_mutex_t map_mutex = PTHREAD_MUTEX_INITIALIZER;

// Optimized hash function
uint64_t hash_linguistic_element(const Token *tokens, size_t token_count)
{
    uint64_t hash = FNV_OFFSET;
    for (size_t i = 0; i < token_count; i++)
    {
        const Token *token = &tokens[i];
        // Hash the token text
        for (size_t j = 0; j < token->length; j++)
        {
            hash ^= (uint64_t)(unsigned char)token->text[j];
            hash *= FNV_PRIME;
        }
        // Hash the token classes (using XOR for all classes)
        uint64_t class_hash = 0;
        for (size_t j = 0; j < token->class_count; j++)
        {
            class_hash ^= (uint64_t)token->classes[j];
        }
        hash ^= class_hash;
        hash *= FNV_PRIME;
    }
    return hash;
}
#include "linguistic_element.h"
#include <string.h>
#include <pthread.h>

#define FNV_PRIME 1099511628211ULL
#define FNV_OFFSET 14695981039346656037ULL
#define LOAD_FACTOR_THRESHOLD 0.75

static pthread_mutex_t map_mutex = PTHREAD_MUTEX_INITIALIZER;

// Hash function remains the same

ErrorCode linguistic_element_map_init(LinguisticElementMap *map, size_t initial_capacity, LimdyMemoryPool *pool)
{
    CHECK_NULL(map, ERROR_NULL_POINTER);
    CHECK_NULL(pool, ERROR_NULL_POINTER);

    map->elements = limdy_memory_pool_alloc_from(pool, initial_capacity * sizeof(ExtendedLinguisticElement));
    if (!map->elements)
    {
        return LIMDY_MEMORY_POOL_ERROR_ALLOC_FAILED;
    }

    map->element_count = 0;
    map->capacity = initial_capacity;
    map->pool = pool;

    memset(map->elements, 0, initial_capacity * sizeof(ExtendedLinguisticElement));

    return ERROR_SUCCESS;
}

static ErrorCode linguistic_element_map_resize(LinguisticElementMap *map)
{
    size_t new_capacity = map->capacity * 2;
    ExtendedLinguisticElement *new_elements = limdy_memory_pool_alloc_from(map->pool, new_capacity * sizeof(ExtendedLinguisticElement));
    if (!new_elements)
    {
        return LIMDY_MEMORY_POOL_ERROR_ALLOC_FAILED;
    }

    memset(new_elements, 0, new_capacity * sizeof(ExtendedLinguisticElement));

    for (size_t i = 0; i < map->capacity; i++)
    {
        if (map->elements[i].base.token_count > 0)
        {
            size_t index = map->elements[i].base.hash % new_capacity;
            size_t step = 1;
            while (new_elements[index].base.token_count > 0)
            {
                index = (index + step * step) % new_capacity; // Quadratic probing
                step++;
            }
            new_elements[index] = map->elements[i];
        }
    }

    limdy_memory_pool_free_to(map->pool, map->elements);
    map->elements = new_elements;
    map->capacity = new_capacity;

    return ERROR_SUCCESS;
}

ErrorCode linguistic_element_map_add(LinguisticElementMap *map, ExtendedLinguisticElement *element)
{
    CHECK_NULL(map, ERROR_NULL_POINTER);
    CHECK_NULL(element, ERROR_NULL_POINTER);

    MUTEX_LOCK(&map_mutex);

    if ((float)map->element_count / map->capacity > LOAD_FACTOR_THRESHOLD)
    {
        ErrorCode err = linguistic_element_map_resize(map);
        if (err != ERROR_SUCCESS)
        {
            MUTEX_UNLOCK(&map_mutex);
            return err;
        }
    }

    size_t index = element->base.hash % map->capacity;
    size_t step = 1;
    while (map->elements[index].base.token_count > 0)
    {
        if (map->elements[index].base.hash == element->base.hash)
        {
            map->elements[index] = *element;
            MUTEX_UNLOCK(&map_mutex);
            return ERROR_SUCCESS;
        }
        index = (index + step * step) % map->capacity; // Quadratic probing
        step++;
        if (step > map->capacity)
        {
            MUTEX_UNLOCK(&map_mutex);
            return LIMDY_LINGUISTIC_ELEMENT_ERROR_MAP_FULL;
        }
    }

    map->elements[index] = *element;
    map->element_count++;

    MUTEX_UNLOCK(&map_mutex);
    return ERROR_SUCCESS;
}

ErrorCode linguistic_element_map_add_occurrence(LinguisticElementMap *map, uint64_t hash, Token **tokens, size_t token_count)
{
    CHECK_NULL(map, ERROR_NULL_POINTER);
    CHECK_NULL(tokens, ERROR_NULL_POINTER);

    MUTEX_LOCK(&map_mutex);

    ExtendedLinguisticElement *element = linguistic_element_map_find(map, hash);
    if (!element)
    {
        MUTEX_UNLOCK(&map_mutex);
        return LIMDY_LINGUISTIC_ELEMENT_ERROR_NOT_FOUND;
    }

    size_t new_count = element->occurrence_count + 1;
    Token ***new_occurrences = limdy_memory_pool_realloc_from(map->pool, element->occurrences, new_count * sizeof(Token **));
    if (!new_occurrences)
    {
        MUTEX_UNLOCK(&map_mutex);
        return LIMDY_MEMORY_POOL_ERROR_ALLOC_FAILED;
    }

    new_occurrences[new_count - 1] = limdy_memory_pool_alloc_from(map->pool, token_count * sizeof(Token *));
    if (!new_occurrences[new_count - 1])
    {
        MUTEX_UNLOCK(&map_mutex);
        return LIMDY_MEMORY_POOL_ERROR_ALLOC_FAILED;
    }

    memcpy(new_occurrences[new_count - 1], tokens, token_count * sizeof(Token *));

    element->occurrences = new_occurrences;
    element->occurrence_count = new_count;

    MUTEX_UNLOCK(&map_mutex);
    return ERROR_SUCCESS;
}

ExtendedLinguisticElement *linguistic_element_map_find(LinguisticElementMap *map, uint64_t hash)
{
    CHECK_NULL(map, NULL);

    MUTEX_LOCK(&map_mutex);

    size_t index = hash % map->capacity;
    size_t step = 1;
    while (map->elements[index].base.token_count > 0)
    {
        if (map->elements[index].base.hash == hash)
        {
            MUTEX_UNLOCK(&map_mutex);
            return &map->elements[index];
        }
        index = (index + step * step) % map->capacity; // Quadratic probing
        step++;
        if (step > map->capacity)
        {
            break;
        }
    }

    MUTEX_UNLOCK(&map_mutex);
    return NULL;
}

void linguistic_element_map_free(LinguisticElementMap *map)
{
    if (!map)
    {
        return;
    }

    MUTEX_LOCK(&map_mutex);

    for (size_t i = 0; i < map->capacity; i++)
    {
        if (map->elements[i].base.token_count > 0)
        {
            limdy_memory_pool_free_to(map->pool, map->elements[i].base.tokens);
            for (size_t j = 0; j < map->elements[i].occurrence_count; j++)
            {
                limdy_memory_pool_free_to(map->pool, map->elements[i].occurrences[j]);
            }
            limdy_memory_pool_free_to(map->pool, map->elements[i].occurrences);
        }
    }

    limdy_memory_pool_free_to(map->pool, map->elements);
    map->elements = NULL;
    map->element_count = 0;
    map->capacity = 0;

    MUTEX_UNLOCK(&map_mutex);
}