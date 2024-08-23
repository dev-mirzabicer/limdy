#ifndef LIMDY_COMPONENTS_RENDERER_LINGUISTIC_ELEMENT_H
#define LIMDY_COMPONENTS_RENDERER_LINGUISTIC_ELEMENT_H

#include <stddef.h>
#include <stdint.h>
#include "limdy_types.h"
#include "error_handler.h"
#include "memory_pool.h"
#include "token.h"
#include "limdy_utils.h"

typedef enum
{
    ELEMENT_VOCAB,
    ELEMENT_PHRASE,
    ELEMENT_SYNTAX
} LinguisticElementType;

// Base LinguisticElement (can be used for both disk and memory)
typedef struct
{
    LinguisticElementType type;
    Token *tokens;
    size_t token_count;
    uint64_t hash; // Pre-computed hash for quick lookup
} LinguisticElement;

// Extended LinguisticElement for in-memory use
typedef struct
{
    LinguisticElement base;
    Token ***occurrences; // Array of arrays of token pointers
    size_t occurrence_count;
} ExtendedLinguisticElement;

typedef struct
{
    ExtendedLinguisticElement *elements;
    size_t element_count;
    size_t capacity;
    LimdyMemoryPool *pool; // Memory pool for this map
} LinguisticElementMap;

// Function prototypes
ErrorCode linguistic_element_map_init(LinguisticElementMap *map, size_t initial_capacity, LimdyMemoryPool *pool);
ErrorCode linguistic_element_map_add(LinguisticElementMap *map, ExtendedLinguisticElement *element);
ErrorCode linguistic_element_map_add_occurrence(LinguisticElementMap *map, uint64_t hash, Token **tokens, size_t token_count);
ExtendedLinguisticElement *linguistic_element_map_find(LinguisticElementMap *map, uint64_t hash);
void linguistic_element_map_free(LinguisticElementMap *map);

// Hash function declaration
uint64_t hash_linguistic_element(const Token *tokens, size_t token_count);

// New error codes
#define LIMDY_LINGUISTIC_ELEMENT_ERROR_BASE (ERROR_CUSTOM_BASE + 200)
#define LIMDY_LINGUISTIC_ELEMENT_ERROR_MAP_FULL (LIMDY_LINGUISTIC_ELEMENT_ERROR_BASE + 1)
#define LIMDY_LINGUISTIC_ELEMENT_ERROR_NOT_FOUND (LIMDY_LINGUISTIC_ELEMENT_ERROR_BASE + 2)

#endif // LIMDY_COMPONENTS_RENDERER_LINGUISTIC_ELEMENT_H