/**
 * @file renderer.h
 * @brief Text rendering and analysis system for the Limdy project.
 *
 * This file defines the structures and functions for the Renderer component,
 * which is responsible for tokenizing, classifying, and extracting linguistic
 * elements from text.
 *
 * @author Mirza Bicer
 * @date 2024-08-23
 */

#ifndef LIMDY_COMPONENTS_RENDERER_H
#define LIMDY_COMPONENTS_RENDERER_H

#include <stddef.h>
#include <stdbool.h>
#include <pthread.h>
#include "error_handler.h"
#include "memory_pool.h"
#include "limdy_types.h"
#include "token.h"
#include "linguistic_element.h"

/**
 * @brief Structure holding the results of rendering.
 */
typedef struct
{
    Token *tokens;
    size_t token_count;
    LinguisticElementMap vocab_map;
    LinguisticElementMap phrase_map;
    LinguisticElementMap syntax_map;
    LimdyMemoryPool *pool;
} RendererResult;

/**
 * @brief Interface for classification services.
 */
typedef struct ClassificationService
{
    ErrorCode (*classify)(Token *tokens, size_t token_count);
    void (*destroy)(struct ClassificationService *service);
} ClassificationService;

/**
 * @brief Structure representing a Renderer.
 */
typedef struct
{
    LimdyMemoryPool *pool;
    TokenizationService *tokenization_service;
    ClassificationService *classification_service;
    pthread_mutex_t mutex;
} Renderer;

/**
 * @brief Create a new Renderer.
 *
 * @param pool Memory pool to use for allocations.
 * @param tokenization_service The tokenization service to use.
 * @param classification_service The classification service to use.
 * @return Pointer to the created Renderer, or NULL on failure.
 */
Renderer *renderer_create(LimdyMemoryPool *pool, TokenizationService *tokenization_service, ClassificationService *classification_service);

/**
 * @brief Destroy a Renderer and free its resources.
 *
 * @param renderer The Renderer to destroy.
 */
void renderer_destroy(Renderer *renderer);

/**
 * @brief Tokenize text in the specified language.
 *
 * This function is thread-safe.
 *
 * @param renderer The Renderer to use.
 * @param text The text to tokenize.
 * @param lang The language of the text.
 * @param result Pointer to store the rendering result.
 * @return ErrorCode indicating success or failure.
 */
ErrorCode renderer_tokenize(Renderer *renderer, const char *text, Language lang, RendererResult *result);

/**
 * @brief Classify tokenized text.
 *
 * This function is thread-safe.
 *
 * @param renderer The Renderer to use.
 * @param result Pointer to the RendererResult to classify.
 * @return ErrorCode indicating success or failure.
 */
ErrorCode renderer_classify(Renderer *renderer, RendererResult *result);

/**
 * @brief Extract linguistic elements (vocab, phrases, syntax) from classified text.
 *
 * This function is thread-safe.
 *
 * @param renderer The Renderer to use.
 * @param result Pointer to the RendererResult to extract elements from.
 * @return ErrorCode indicating success or failure.
 */
ErrorCode renderer_extract_elements(Renderer *renderer, RendererResult *result);

/**
 * @brief Perform full rendering (tokenization, classification, and extraction) on text.
 *
 * This function is thread-safe.
 *
 * @param renderer The Renderer to use.
 * @param text The text to render.
 * @param lang The language of the text.
 * @param result Pointer to store the rendering result.
 * @return ErrorCode indicating success or failure.
 */
ErrorCode renderer_render(Renderer *renderer, const char *text, Language lang, RendererResult *result);

/**
 * @brief Free the resources of a RendererResult.
 *
 * This function is thread-safe.
 *
 * @param renderer The Renderer that created the result.
 * @param result The RendererResult to free.
 */
void renderer_free_result(Renderer *renderer, RendererResult *result);

#endif // LIMDY_COMPONENTS_RENDERER_H