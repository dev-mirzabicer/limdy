/**
 * @file renderer.c
 * @brief Implementation of the Renderer component for the Limdy project.
 *
 * This file contains the implementation of the Renderer component, which is
 * responsible for tokenizing, classifying, and extracting linguistic elements
 * from text.
 *
 * @author Mirza Bicer
 * @date 2024-08-23
 */

#include <stdlib.h>
#include <string.h>
#include "renderer.h"
#include "limdy_utils.h"

/**
 * @brief Create a new Renderer.
 *
 * This function initializes a new Renderer with the given memory pool and services.
 * It also initializes the mutex for thread safety.
 *
 * @param pool Memory pool to use for allocations.
 * @param tokenization_service The tokenization service to use.
 * @param classification_service The classification service to use.
 * @return Pointer to the created Renderer, or NULL on failure.
 */
Renderer *renderer_create(LimdyMemoryPool *pool, TokenizationService *tokenization_service, ClassificationService *classification_service)
{
    CHECK_NULL(pool, ERROR_NULL_POINTER);
    CHECK_NULL(tokenization_service, ERROR_NULL_POINTER);
    CHECK_NULL(classification_service, ERROR_NULL_POINTER);

    Renderer *renderer = limdy_memory_pool_alloc_from(pool, sizeof(Renderer));
    CHECK_NULL(renderer, ERROR_MEMORY_ALLOCATION);

    renderer->pool = pool;
    renderer->tokenization_service = tokenization_service;
    renderer->classification_service = classification_service;

    if (pthread_mutex_init(&renderer->mutex, NULL) != 0)
    {
        LOG_ERROR(ERROR_THREAD_INIT, "Failed to initialize mutex for Renderer");
        limdy_memory_pool_free_to(pool, renderer);
        return NULL;
    }

    return renderer;
}

/**
 * @brief Destroy a Renderer and free its resources.
 *
 * This function destroys the given Renderer, freeing all associated resources,
 * including the tokenization and classification services. It also destroys
 * the mutex used for thread safety.
 *
 * @param renderer The Renderer to destroy.
 */
void renderer_destroy(Renderer *renderer)
{
    CHECK_NULL(renderer, ERROR_NULL_POINTER);

    if (pthread_mutex_destroy(&renderer->mutex) != 0)
    {
        LOG_ERROR(ERROR_THREAD_UNLOCK, "Failed to destroy mutex for Renderer");
        // Continue with destruction even if mutex destruction fails
    }

    // Destroy the tokenization service
    if (renderer->tokenization_service)
    {
        if (renderer->tokenization_service->destroy)
        {
            renderer->tokenization_service->destroy(renderer->tokenization_service);
        }
        limdy_memory_pool_free_to(renderer->pool, renderer->tokenization_service);
    }

    // Destroy the classification service
    if (renderer->classification_service)
    {
        if (renderer->classification_service->destroy)
        {
            renderer->classification_service->destroy(renderer->classification_service);
        }
        limdy_memory_pool_free_to(renderer->pool, renderer->classification_service);
    }

    limdy_memory_pool_free_to(renderer->pool, renderer);
}

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
ErrorCode renderer_tokenize(Renderer *renderer, const char *text, Language lang, RendererResult *result)
{
    CHECK_NULL(renderer, ERROR_NULL_POINTER);
    CHECK_NULL(text, ERROR_NULL_POINTER);
    CHECK_NULL(result, ERROR_NULL_POINTER);

    ErrorCode error = ERROR_SUCCESS;

    pthread_mutex_lock(&renderer->mutex);

    do
    {
        if (!renderer->tokenization_service || !renderer->tokenization_service->tokenize)
        {
            error = ERROR_RENDERER_SERVICE_UNAVAILABLE;
            break;
        }

        error = renderer->tokenization_service->tokenize(text, lang, &result->tokens, &result->token_count);
        if (error != ERROR_SUCCESS)
        {
            break;
        }

        // Allocate memory for tokens from the result's pool
        Token *pooled_tokens = limdy_memory_pool_alloc_from(result->pool, sizeof(Token) * result->token_count);
        if (!pooled_tokens)
        {
            error = ERROR_MEMORY_ALLOCATION;
            break;
        }

        // Copy tokens to the pooled memory
        for (size_t i = 0; i < result->token_count; i++)
        {
            pooled_tokens[i] = result->tokens[i];
            pooled_tokens[i].text = limdy_memory_pool_alloc_from(result->pool, result->tokens[i].length + 1);
            if (!pooled_tokens[i].text)
            {
                error = ERROR_MEMORY_ALLOCATION;
                break;
            }
            memcpy(pooled_tokens[i].text, result->tokens[i].text, result->tokens[i].length + 1);
        }

        if (error == ERROR_SUCCESS)
        {
            // Free the original tokens and replace with pooled tokens
            renderer->tokenization_service->free_tokens(result->tokens, result->token_count);
            result->tokens = pooled_tokens;
        }
        else
        {
            // Clean up on error
            for (size_t i = 0; i < result->token_count; i++)
            {
                if (pooled_tokens[i].text)
                {
                    limdy_memory_pool_free_to(result->pool, pooled_tokens[i].text);
                }
            }
            limdy_memory_pool_free_to(result->pool, pooled_tokens);
        }

    } while (0);

    pthread_mutex_unlock(&renderer->mutex);

    return error;
}

/**
 * @brief Classify tokenized text.
 *
 * This function is thread-safe.
 *
 * @param renderer The Renderer to use.
 * @param result Pointer to the RendererResult to classify.
 * @return ErrorCode indicating success or failure.
 */
ErrorCode renderer_classify(Renderer *renderer, RendererResult *result)
{
    CHECK_NULL(renderer, ERROR_NULL_POINTER);
    CHECK_NULL(result, ERROR_NULL_POINTER);
    CHECK_NULL(result->tokens, ERROR_NULL_POINTER);

    ErrorCode error = ERROR_SUCCESS;

    pthread_mutex_lock(&renderer->mutex);

    do
    {
        if (!renderer->classification_service || !renderer->classification_service->classify)
        {
            error = ERROR_RENDERER_SERVICE_UNAVAILABLE;
            break;
        }

        error = renderer->classification_service->classify(result->tokens, result->token_count);
        if (error != ERROR_SUCCESS)
        {
            break;
        }

    } while (0);

    pthread_mutex_unlock(&renderer->mutex);

    return error;
}

/**
 * @brief Extract linguistic elements (vocab, phrases, syntax) from classified text.
 *
 * This function is thread-safe.
 *
 * @param renderer The Renderer to use.
 * @param result Pointer to the RendererResult to extract elements from.
 * @return ErrorCode indicating success or failure.
 */
ErrorCode renderer_extract_elements(Renderer *renderer, RendererResult *result)
{
    CHECK_NULL(renderer, ERROR_NULL_POINTER);
    CHECK_NULL(result, ERROR_NULL_POINTER);
    CHECK_NULL(result->tokens, ERROR_NULL_POINTER);

    ErrorCode error = ERROR_SUCCESS;

    pthread_mutex_lock(&renderer->mutex);

    do
    {
        // Initialize linguistic element maps
        error = linguistic_element_map_init(&result->vocab_map, result->token_count, result->pool);
        if (error != ERROR_SUCCESS)
            break;

        error = linguistic_element_map_init(&result->phrase_map, result->token_count / 2, result->pool);
        if (error != ERROR_SUCCESS)
            break;

        error = linguistic_element_map_init(&result->syntax_map, result->token_count / 2, result->pool);
        if (error != ERROR_SUCCESS)
            break;

        // Extract vocab (single tokens)
        for (size_t i = 0; i < result->token_count; i++)
        {
            Token **token_ptr = limdy_memory_pool_alloc_from(result->pool, sizeof(Token *));
            if (!token_ptr)
            {
                error = ERROR_MEMORY_ALLOCATION;
                break;
            }
            *token_ptr = &result->tokens[i];

            TypedLinguisticElement element = {
                .type = ELEMENT_VOCAB,
                .element = {
                    .tokens = token_ptr,
                    .token_count = 1,
                    .hash = hash_token(&result->tokens[i]) // Implement this hash function
                }};
            error = linguistic_element_map_add(&result->vocab_map, &element);
            if (error != ERROR_SUCCESS)
                break;
        }

        // TODO: Implement phrase and syntax extraction
        // This would involve analyzing the tokens to identify phrases and syntactic structures
        // You'll need to implement the logic for creating and hashing these more complex structures

    } while (0);

    pthread_mutex_unlock(&renderer->mutex);

    return error;
}

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
ErrorCode renderer_render(Renderer *renderer, const char *text, Language lang, RendererResult *result)
{
    CHECK_NULL(renderer, ERROR_NULL_POINTER);
    CHECK_NULL(text, ERROR_NULL_POINTER);
    CHECK_NULL(result, ERROR_NULL_POINTER);

    ErrorCode error;

    // Initialize result
    memset(result, 0, sizeof(RendererResult));

    // Tokenize
    error = renderer_tokenize(renderer, text, lang, result);
    if (error != ERROR_SUCCESS)
    {
        renderer_free_result(renderer, result);
        return error;
    }

    // Classify
    error = renderer_classify(renderer, result);
    if (error != ERROR_SUCCESS)
    {
        renderer_free_result(renderer, result);
        return error;
    }

    // Extract elements
    error = renderer_extract_elements(renderer, result);
    if (error != ERROR_SUCCESS)
    {
        renderer_free_result(renderer, result);
        return error;
    }

    return ERROR_SUCCESS;
}

/**
 * @brief Free the resources of a RendererResult.
 *
 * This function is thread-safe.
 *
 * @param renderer The Renderer that created the result.
 * @param result The RendererResult to free.
 */
void renderer_free_result(Renderer *renderer, RendererResult *result)
{
    CHECK_NULL(renderer, ERROR_NULL_POINTER);
    CHECK_NULL(result, ERROR_NULL_POINTER);

    pthread_mutex_lock(&renderer->mutex);

    linguistic_element_map_free(&result->vocab_map);
    linguistic_element_map_free(&result->phrase_map);
    linguistic_element_map_free(&result->syntax_map);

    if (result->pool)
    {
        limdy_memory_pool_destroy(result->pool);
        result->pool = NULL;
    }

    result->tokens = NULL;
    result->token_count = 0;

    pthread_mutex_unlock(&renderer->mutex);
}
