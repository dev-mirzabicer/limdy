#ifndef LIMDY_COMPONENTS_RENDERER_LINGUISTIC_ELEMENT_H
#define LIMDY_COMPONENTS_RENDERER_LINGUISTIC_ELEMENT_H

#include <stddef.h>
#include <stdint.h>
#include "limdy_types.h"
#include "error_handler.h"
#include "memory_pool.h"
#include "token.h"

/**
 * @brief Structure representing a linguistic element (Vocab, Phrase, or Syntax).
 */
typedef struct
{
    Token **tokens;
    size_t token_count;
    uint64_t hash; // Hash for quick lookup
} LinguisticElement;

/**
 * @brief Enumeration of linguistic element types.
 */
typedef enum
{
    ELEMENT_VOCAB,
    ELEMENT_PHRASE,
    ELEMENT_SYNTAX
} LinguisticElementType;

/**
 * @brief Structure representing a linguistic element with its type.
 */
typedef struct
{
    LinguisticElementType type;
    LinguisticElement element;
} TypedLinguisticElement;

/**
 * @brief Structure for efficient storage and lookup of linguistic elements.
 */
typedef struct
{
    TypedLinguisticElement *elements;
    size_t element_count;
    size_t capacity;
    LimdyMemoryPool *pool; // Memory pool for this map
} LinguisticElementMap;

/**
 * @brief Initialize a LinguisticElementMap.
 *
 * @param map Pointer to the LinguisticElementMap to initialize.
 * @param initial_capacity Initial capacity of the map.
 * @param pool Memory pool to use for allocations.
 * @return ErrorCode indicating success or failure.
 */
ErrorCode linguistic_element_map_init(LinguisticElementMap *map, size_t initial_capacity, LimdyMemoryPool *pool);

/**
 * @brief Add a linguistic element to the map.
 *
 * @param map Pointer to the LinguisticElementMap.
 * @param element Pointer to the TypedLinguisticElement to add.
 * @return ErrorCode indicating success or failure.
 */
ErrorCode linguistic_element_map_add(LinguisticElementMap *map, TypedLinguisticElement *element);

/**
 * @brief Find a linguistic element in the map.
 *
 * @param map Pointer to the LinguisticElementMap.
 * @param hash Hash of the element to find.
 * @return Pointer to the found TypedLinguisticElement, or NULL if not found.
 */
TypedLinguisticElement *linguistic_element_map_find(LinguisticElementMap *map, uint64_t hash);

/**
 * @brief Free resources used by a LinguisticElementMap.
 *
 * @param map Pointer to the LinguisticElementMap to free.
 */
void linguistic_element_map_free(LinguisticElementMap *map);

#endif // LIMDY_COMPONENTS_RENDERER_LINGUISTIC_ELEMENT_H