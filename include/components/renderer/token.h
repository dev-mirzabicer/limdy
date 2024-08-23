#ifndef LIMDY_COMPONENTS_RENDERER_TOKEN_H
#define LIMDY_COMPONENTS_RENDERER_TOKEN_H

#include <stddef.h>
#include "limdy_types.h"

#define TOKEN_PLACEHOLDER ((char *)1)

/**
 * @brief Enumeration of token classes.
 */
typedef enum
{
    CLS_NOUN,
    CLS_VERB,
    CLS_ADJECTIVE,
    // Add more classes as needed
    CLS_COUNT
} TokenClass;

/**
 * @brief Structure representing a token.
 */
typedef struct
{
    char *text;
    size_t length;
    TokenClass *classes;
    size_t class_count;
} Token;

/**
 * @brief Interface for tokenization services.
 */
typedef struct TokenizationService
{
    ErrorCode (*tokenize)(const char *text, Language lang, Token **tokens, size_t *token_count);
    void (*free_tokens)(Token *tokens, size_t token_count);
    void (*destroy)(struct TokenizationService *service);
} TokenizationService;

#endif // LIMDY_COMPONENTS_RENDERER_TOKEN_H