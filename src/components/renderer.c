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
#include "error_handler.h"

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
    if (!pool || !tokenization_service || !classification_service)
    {
        LOG_ERROR(ERROR_NULL_POINTER, "Null pointer passed to renderer_create");
        return NULL;
    }

    Renderer *renderer = limdy_memory_pool_alloc_from(pool, sizeof(Renderer));
    if (!renderer)
    {
        LOG_ERROR(ERROR_MEMORY_ALLOCATION, "Failed to allocate memory for Renderer");
        return NULL;
    }

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
    if (!renderer)
    {
        LOG_ERROR(ERROR_NULL_POINTER, "Null pointer passed to renderer_destroy");
        return;
    }

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
    if (!renderer || !text || !result)
    {
        return ERROR_NULL_POINTER;
    }

    ErrorCode error = ERROR_SUCCESS;

    pthread_mutex_lock(&renderer->mutex);

    do
    {
        if (!renderer->tokenization_service || !renderer->tokenization_service->tokenize)
        {
            error = ERROR_RENDERER_SERVICE_UNAVAILABLE;
            break;
        }

        Token *tokens = NULL;
        size_t token_count = 0;

        error = renderer->tokenization_service->tokenize(text, lang, &tokens, &token_count);
        if (error != ERROR_SUCCESS)
        {
            break;
        }

        // Allocate memory for the tokens in the result
        result->tokens = limdy_memory_pool_alloc_from(renderer->pool, sizeof(Token) * token_count);
        if (!result->tokens)
        {
            error = ERROR_MEMORY_ALLOCATION;
            break;
        }

        // Copy tokens to the result
        memcpy(result->tokens, tokens, sizeof(Token) * token_count);
        result->token_count = token_count;

        // Free the tokens returned by the service
        renderer->tokenization_service->free_tokens(tokens, token_count);

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
    if (!renderer || !result || !result->tokens)
    {
        return ERROR_NULL_POINTER;
    }

    ErrorCode error = ERROR_SUCCESS;

    pthread_mutex_lock(&renderer->mutex);

    do
    {
        if (!renderer->classification_service || !renderer->classification_service->classify)
        {
            error = ERROR_RENDERER_SERVICE_UNAVAILABLE;
            break;
        }

        ClassifiedToken *classified_tokens = NULL;
        size_t classified_token_count = 0;

        error = renderer->classification_service->classify(result->tokens, result->token_count, &classified_tokens, &classified_token_count);
        if (error != ERROR_SUCCESS)
        {
            break;
        }

        // Allocate memory for the classified tokens in the result
        result->classified_tokens = limdy_memory_pool_alloc_from(renderer->pool, sizeof(ClassifiedToken) * classified_token_count);
        if (!result->classified_tokens)
        {
            error = ERROR_MEMORY_ALLOCATION;
            break;
        }

        // Copy classified tokens to the result
        memcpy(result->classified_tokens, classified_tokens, sizeof(ClassifiedToken) * classified_token_count);
        result->classified_token_count = classified_token_count;

        // Free the classified tokens returned by the service
        renderer->classification_service->free_classified_tokens(classified_tokens, classified_token_count);

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
    if (!renderer || !result || !result->classified_tokens)
    {
        return ERROR_NULL_POINTER;
    }

    ErrorCode error = ERROR_SUCCESS;

    pthread_mutex_lock(&renderer->mutex);

    do
    {
        // Allocate memory for elements
        result->elements = limdy_memory_pool_alloc_from(renderer->pool, sizeof(TypedLinguisticElement) * result->classified_token_count);
        if (!result->elements)
        {
            error = ERROR_MEMORY_ALLOCATION;
            break;
        }

        result->element_count = 0;

        // Extract vocab (single tokens)
        for (size_t i = 0; i < result->classified_token_count; i++)
        {
            TypedLinguisticElement *element = &result->elements[result->element_count++];
            element->type = ELEMENT_VOCAB;
            element->element.vocab = result->classified_tokens[i].token;
        }

        // TODO: Implement phrase and syntax extraction
        // This would involve analyzing the classified tokens to identify phrases and syntactic structures
        // For now, we'll leave this as a placeholder

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
    if (!renderer || !text || !result)
    {
        return ERROR_NULL_POINTER;
    }

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
    if (!renderer || !result)
    {
        return;
    }

    pthread_mutex_lock(&renderer->mutex);

    if (result->tokens)
    {
        limdy_memory_pool_free_to(renderer->pool, result->tokens);
        result->tokens = NULL;
        result->token_count = 0;
    }

    if (result->classified_tokens)
    {
        for (size_t i = 0; i < result->classified_token_count; i++)
        {
            if (result->classified_tokens[i].classes)
            {
                limdy_memory_pool_free_to(renderer->pool, result->classified_tokens[i].classes);
            }
        }
        limdy_memory_pool_free_to(renderer->pool, result->classified_tokens);
        result->classified_tokens = NULL;
        result->classified_token_count = 0;
    }

    if (result->elements)
    {
        limdy_memory_pool_free_to(renderer->pool, result->elements);
        result->elements = NULL;
        result->element_count = 0;
    }

    pthread_mutex_unlock(&renderer->mutex);
}