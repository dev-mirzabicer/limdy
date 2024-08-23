#include <stdlib.h>
#include <string.h>
#include "error_handler.h"
#include "limdy_utils.h"
#include "linguistic_element.h"
#include "token.h"

ErrorCode linguistic_element_map_init(LinguisticElementMap *map, size_t initial_capacity, LimdyMemoryPool *pool)
{
    CHECK_NULL(map, ERROR_NULL_POINTER);
    CHECK_NULL(pool, ERROR_NULL_POINTER);

    map->elements = limdy_memory_pool_alloc_from(pool, sizeof(TypedLinguisticElement) * initial_capacity);
    if (!map->elements)
    {
        return ERROR_MEMORY_ALLOCATION;
    }

    map->element_count = 0;
    map->capacity = initial_capacity;
    map->pool = pool;

    return ERROR_SUCCESS;
}

ErrorCode linguistic_element_map_add(LinguisticElementMap *map, TypedLinguisticElement *element)
{
    CHECK_NULL(map, ERROR_NULL_POINTER);
    CHECK_NULL(element, ERROR_NULL_POINTER);

    if (map->element_count >= map->capacity)
    {
        size_t new_capacity = map->capacity * 2;
        TypedLinguisticElement *new_elements = limdy_memory_pool_alloc_from(map->pool, sizeof(TypedLinguisticElement) * new_capacity);
        if (!new_elements)
        {
            return ERROR_MEMORY_ALLOCATION;
        }
        memcpy(new_elements, map->elements, sizeof(TypedLinguisticElement) * map->element_count);
        limdy_memory_pool_free_to(map->pool, map->elements);
        map->elements = new_elements;
        map->capacity = new_capacity;
    }

    map->elements[map->element_count++] = *element;
    return ERROR_SUCCESS;
}

void linguistic_element_map_free(LinguisticElementMap *map)
{
    CHECK_NULL(map, NULL);

    for (size_t i = 0; i < map->element_count; i++)
    {
        limdy_memory_pool_free_to(map->pool, map->elements[i].element.tokens);
    }
    limdy_memory_pool_free_to(map->pool, map->elements);
    map->elements = NULL;
    map->element_count = 0;
    map->capacity = 0;
    map->pool = NULL;
}