/**
 * @file translator_aligner.c
 * @brief Implementation of the translation and alignment system for the Limdy project.
 *
 * This file implements the functions declared in translator_aligner.h. It provides
 * the core functionality for translating text and aligning the translated text
 * with the original.
 *
 * @author Mirza Bicer
 * @date 2024-08-23
 */

#include "components/translator_aligner.h"
#include <stdlib.h>
#include <string.h>
#include "utils/limdy_utils.h"
#include "utils/memory_pool.h"

/**
 * @brief Creates a new Translator instance.
 *
 * @param service The translation service to use.
 * @return A pointer to the new Translator, or NULL if creation fails.
 */
Translator *translator_create(TranslationService *service)
{
    CHECK_NULL(service, ERROR_NULL_POINTER);

    Translator *translator = limdy_memory_pool_alloc(sizeof(Translator));
    CHECK_NULL(translator, ERROR_MEMORY_ALLOCATION);

    translator->service = service;

    if (pthread_mutex_init(&translator->mutex, NULL) != 0)
    {
        LOG_ERROR(ERROR_THREAD_INIT, "Failed to initialize translator mutex");
        limdy_memory_pool_free(translator);
        return NULL;
    }

    translator->pool = NULL;
    ErrorCode error = limdy_memory_pool_create(LIMDY_SMALL_POOL_SIZE, &translator->pool);
    if (error != ERROR_SUCCESS)
    {
        pthread_mutex_destroy(&translator->mutex);
        limdy_memory_pool_free(translator);
        LOG_ERROR(error, "Failed to create memory pool for translator");
        return NULL;
    }

    return translator;
}

/**
 * @brief Destroys a Translator instance and frees its resources.
 *
 * @param translator The Translator to destroy.
 */
void translator_destroy(Translator *translator)
{
    if (translator)
    {
        pthread_mutex_destroy(&translator->mutex);
        if (translator->pool)
        {
            limdy_memory_pool_destroy(translator->pool);
        }
        limdy_memory_pool_free(translator);
    }
}

/**
 * @brief Performs a translation operation.
 *
 * This function translates the given text from the source language to the target language
 * and generates an attention matrix for alignment.
 *
 * @param translator The Translator to use.
 * @param text The text to translate.
 * @param source_lang The source language.
 * @param target_lang The target language.
 * @param result Pointer to store the translation result.
 * @return ErrorCode indicating success or failure.
 */
ErrorCode translator_translate(Translator *translator, const char *text, const char *source_lang, const char *target_lang, TranslationResult *result)
{
    CHECK_NULL(translator, ERROR_NULL_POINTER);
    CHECK_NULL(text, ERROR_NULL_POINTER);
    CHECK_NULL(source_lang, ERROR_NULL_POINTER);
    CHECK_NULL(target_lang, ERROR_NULL_POINTER);
    CHECK_NULL(result, ERROR_NULL_POINTER);

    MUTEX_LOCK(&translator->mutex);

    ErrorCode error = allocate_translation_result(result, LIMDY_LARGE_POOL_SIZE); // TODO Implement flexible pool size
    if (error != ERROR_SUCCESS)
    {
        MUTEX_UNLOCK(&translator->mutex);
        return error;
    }

    error = translator->service->translate(text, source_lang, target_lang, &result->translated_text);
    if (error != ERROR_SUCCESS)
    {
        LOG_ERROR(error, "Translation failed");
        free_translation_result(result);
        MUTEX_UNLOCK(&translator->mutex);
        return error;
    }

    error = translator->service->get_attention_matrix(text, result->translated_text, &result->attention_matrix, &result->rows, &result->cols);
    if (error != ERROR_SUCCESS)
    {
        LOG_ERROR(error, "Failed to get attention matrix");
        free_translation_result(result);
        MUTEX_UNLOCK(&translator->mutex);
        return error;
    }

    MUTEX_UNLOCK(&translator->mutex);
    return ERROR_SUCCESS;
}

/**
 * @brief Creates a new Aligner instance.
 *
 * @param service The alignment service to use.
 * @param renderer The renderer to use for tokenization.
 * @return A pointer to the new Aligner, or NULL if creation fails.
 */
Aligner *aligner_create(AlignmentService *service, Renderer *renderer)
{
    CHECK_NULL(service, NULL);
    CHECK_NULL(renderer, NULL);

    Aligner *aligner = limdy_memory_pool_alloc(sizeof(Aligner));
    CHECK_NULL(aligner, NULL);

    aligner->service = service;
    aligner->renderer = renderer;

    if (pthread_mutex_init(&aligner->mutex, NULL) != 0)
    {
        LOG_ERROR(ERROR_THREAD_INIT, "Failed to initialize aligner mutex");
        limdy_memory_pool_free(aligner);
        return NULL;
    }

    aligner->pool = NULL;
    ErrorCode error = limdy_memory_pool_create(LIMDY_SMALL_POOL_SIZE, &aligner->pool);
    if (error != ERROR_SUCCESS)
    {
        pthread_mutex_destroy(&aligner->mutex);
        limdy_memory_pool_free(aligner);
        LOG_ERROR(error, "Failed to create memory pool for aligner");
        return NULL;
    }

    return aligner;
}

/**
 * @brief Destroys an Aligner instance and frees its resources.
 *
 * @param aligner The Aligner to destroy.
 */
void aligner_destroy(Aligner *aligner)
{
    if (aligner)
    {
        pthread_mutex_destroy(&aligner->mutex);
        if (aligner->pool)
        {
            limdy_memory_pool_destroy(aligner->pool);
        }
        limdy_memory_pool_free(aligner);
    }
}

/**
 * @brief Performs an alignment operation.
 *
 * This function aligns the source text with the target (translated) text using
 * the provided attention matrix.
 *
 * @param aligner The Aligner to use.
 * @param source_text The source text.
 * @param target_text The target (translated) text.
 * @param attention_matrix The attention matrix.
 * @param rows Number of rows in the attention matrix.
 * @param cols Number of columns in the attention matrix.
 * @param aligned_text Pointer to store the aligned text.
 * @param aligned_size Pointer to store the size of the aligned text.
 * @return ErrorCode indicating success or failure.
 */
ErrorCode aligner_align(Aligner *aligner, const char *source_text, const char *target_text, float **attention_matrix, size_t rows, size_t cols, char ***aligned_text, size_t *aligned_size)
{
    CHECK_NULL(aligner, ERROR_NULL_POINTER);
    CHECK_NULL(source_text, ERROR_NULL_POINTER);
    CHECK_NULL(target_text, ERROR_NULL_POINTER);
    CHECK_NULL(attention_matrix, ERROR_NULL_POINTER);
    CHECK_NULL(aligned_text, ERROR_NULL_POINTER);
    CHECK_NULL(aligned_size, ERROR_NULL_POINTER);

    MUTEX_LOCK(&aligner->mutex);

    RendererResult source_result = {0};
    RendererResult target_result = {0};
    int *alignment = NULL;
    size_t alignment_size = 0;
    ErrorCode error = ERROR_SUCCESS;

    // Tokenize source and target text
    error = renderer_tokenize(aligner->renderer, source_text, LANG_ENGLISH, &source_result); // Assume English for now
    if (error != ERROR_SUCCESS)
    {
        LOG_ERROR(error, "Failed to tokenize source text");
        goto cleanup;
    }

    error = renderer_tokenize(aligner->renderer, target_text, LANG_ENGLISH, &target_result); // Assume English for now
    if (error != ERROR_SUCCESS)
    {
        LOG_ERROR(error, "Failed to tokenize target text");
        goto cleanup;
    }

    // Align tokens
    error = aligner->service->align_tokens((const char **)source_result.tokens, source_result.token_count,
                                           (const char **)target_result.tokens, target_result.token_count,
                                           attention_matrix, rows, cols,
                                           &alignment, &alignment_size);
    if (error != ERROR_SUCCESS)
    {
        LOG_ERROR(error, "Token alignment failed");
        goto cleanup;
    }

    // Create aligned text
    *aligned_text = limdy_memory_pool_alloc_from(aligner->pool, alignment_size * sizeof(char *));
    if (!*aligned_text)
    {
        LOG_ERROR(ERROR_MEMORY_ALLOCATION, "Failed to allocate memory for aligned text");
        error = ERROR_MEMORY_ALLOCATION;
        goto cleanup;
    }

    for (size_t i = 0; i < alignment_size; i++)
    {
        size_t len = source_result.tokens[i].length + target_result.tokens[alignment[i]].length + 4; // 4 for "[", "]", " ", and null terminator
        (*aligned_text)[i] = limdy_memory_pool_alloc_from(aligner->pool, len * sizeof(char));
        if (!(*aligned_text)[i])
        {
            LOG_ERROR(ERROR_MEMORY_ALLOCATION, "Failed to allocate memory for aligned text entry");
            error = ERROR_MEMORY_ALLOCATION;
            goto cleanup;
        }
        snprintf((*aligned_text)[i], len, "[%.*s] [%.*s]",
                 (int)source_result.tokens[i].length, source_result.tokens[i].text,
                 (int)target_result.tokens[alignment[i]].length, target_result.tokens[alignment[i]].text);
    }

    *aligned_size = alignment_size;

cleanup:
    renderer_free_result(aligner->renderer, &source_result);
    renderer_free_result(aligner->renderer, &target_result);
    limdy_memory_pool_free_to(aligner->pool, alignment);

    if (error != ERROR_SUCCESS && *aligned_text)
    {
        for (size_t i = 0; i < alignment_size; i++)
        {
            limdy_memory_pool_free_to(aligner->pool, (*aligned_text)[i]);
        }
        limdy_memory_pool_free_to(aligner->pool, *aligned_text);
        *aligned_text = NULL;
        *aligned_size = 0;
    }

    MUTEX_UNLOCK(&aligner->mutex);
    return error;
}

/**
 * @brief Creates a new TranslatorAligner instance.
 *
 * @param trans_service The translation service to use.
 * @param align_service The alignment service to use.
 * @param renderer The renderer to use for tokenization.
 * @return A pointer to the new TranslatorAligner, or NULL if creation fails.
 */
TranslatorAligner *translator_aligner_create(TranslationService *trans_service, AlignmentService *align_service, Renderer *renderer)
{
    CHECK_NULL(trans_service, NULL);
    CHECK_NULL(align_service, NULL);
    CHECK_NULL(renderer, NULL);
    TranslatorAligner *ta = limdy_memory_pool_alloc(sizeof(TranslatorAligner));
    CHECK_NULL(ta, NULL);

    ta->translator = translator_create(trans_service);
    if (!ta->translator)
    {
        LOG_ERROR(ERROR_UNKNOWN, "Failed to create Translator");
        limdy_memory_pool_free(ta);
        return NULL;
    }

    ta->aligner = aligner_create(align_service, renderer);
    if (!ta->aligner)
    {
        LOG_ERROR(ERROR_UNKNOWN, "Failed to create Aligner");
        translator_destroy(ta->translator);
        limdy_memory_pool_free(ta);
        return NULL;
    }

    if (pthread_mutex_init(&ta->mutex, NULL) != 0)
    {
        LOG_ERROR(ERROR_THREAD_INIT, "Failed to initialize translator_aligner mutex");
        translator_destroy(ta->translator);
        aligner_destroy(ta->aligner);
        limdy_memory_pool_free(ta);
        return NULL;
    }

    ErrorCode error = limdy_memory_pool_create(LIMDY_LARGE_POOL_SIZE, &ta->pool); // TODO Implement flexible pool size
    if (error != ERROR_SUCCESS)
    {
        pthread_mutex_destroy(&ta->mutex);
        translator_destroy(ta->translator);
        aligner_destroy(ta->aligner);
        limdy_memory_pool_free(ta);
        LOG_ERROR(error, "Failed to create memory pool for translator_aligner");
        return NULL;
    }

    return ta;
}

/**
 * @brief Destroys a TranslatorAligner instance and frees its resources.
 *
 * @param ta The TranslatorAligner to destroy.
 */
void translator_aligner_destroy(TranslatorAligner *ta)
{
    if (ta)
    {
        translator_destroy(ta->translator);
        aligner_destroy(ta->aligner);
        pthread_mutex_destroy(&ta->mutex);
        limdy_memory_pool_destroy(ta->pool);
        limdy_memory_pool_free(ta);
    }
}

/**
 * @brief Performs a combined translation and alignment operation.
 *
 * This function translates the given text and then aligns it with the original text.
 *
 * @param ta The TranslatorAligner to use.
 * @param text The text to translate and align.
 * @param source_lang The source language.
 * @param target_lang The target language.
 * @param aligned_text Pointer to store the aligned text.
 * @param aligned_size Pointer to store the size of the aligned text.
 * @return ErrorCode indicating success or failure.
 */
ErrorCode translator_aligner_process(TranslatorAligner *ta, const char *text, const char *source_lang, const char *target_lang, char ***aligned_text, size_t *aligned_size)
{
    CHECK_NULL(ta, ERROR_NULL_POINTER);
    CHECK_NULL(text, ERROR_NULL_POINTER);
    CHECK_NULL(source_lang, ERROR_NULL_POINTER);
    CHECK_NULL(target_lang, ERROR_NULL_POINTER);
    CHECK_NULL(aligned_text, ERROR_NULL_POINTER);
    CHECK_NULL(aligned_size, ERROR_NULL_POINTER);
    MUTEX_LOCK(&ta->mutex);

    TranslationResult result = {0};
    ErrorCode error = translator_translate(ta->translator, text, source_lang, target_lang, &result);
    if (error != ERROR_SUCCESS)
    {
        LOG_ERROR(error, "Translation failed in translator_aligner_process");
        MUTEX_UNLOCK(&ta->mutex);
        return error;
    }

    error = aligner_align(ta->aligner, text, result.translated_text, result.attention_matrix, result.rows, result.cols, aligned_text, aligned_size);
    if (error != ERROR_SUCCESS)
    {
        LOG_ERROR(error, "Alignment failed in translator_aligner_process");
    }

    free_translation_result(&result);

    MUTEX_UNLOCK(&ta->mutex);
    return error;
}

ErrorCode allocate_translation_result(TranslationResult *result, size_t pool_size)
{
    if (!result)
    {
        return ERROR_NULL_POINTER;
    }

    memset(result, 0, sizeof(TranslationResult));

    ErrorCode error = limdy_memory_pool_create(pool_size, &result->pool);
    if (error != ERROR_SUCCESS)
    {
        LOG_ERROR(error, "Failed to create memory pool for translation result");
        return error;
    }

    return ERROR_SUCCESS;
}

/**
 * @brief Frees the resources of a translation result.
 *
 * @param result The translation result to free.
 */
void free_translation_result(TranslationResult *result)
{
    if (result)
    {
        if (result->pool)
        {
            limdy_memory_pool_destroy(result->pool);
        }
        memset(result, 0, sizeof(TranslationResult));
    }
}

/**
 * @brief Frees the resources of aligned text.
 *
 * @param aligned_text The aligned text to free.
 * @param size The size of the aligned text.
 */
void free_aligned_text(char **aligned_text, size_t size)
{
    if (aligned_text)
    {
        for (size_t i = 0; i < size; i++)
        {
            limdy_memory_pool_free(aligned_text[i]);
        }
        limdy_memory_pool_free(aligned_text);
    }
}