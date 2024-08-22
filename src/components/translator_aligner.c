#include "components/translator_aligner.h"
#include <stdlib.h>
#include <string.h>
#include "utils/limdy_utils.h"

// Helper function to allocate 2D float array using memory pool
static float **allocate_2d_float_array(LimdyMemoryPool *pool, size_t rows, size_t cols)
{
    float **array = limdy_memory_pool_alloc_from(pool, rows * sizeof(float *));
    CHECK_NULL(array, ERROR_MEMORY_POOL_ALLOC_FAILED);

    for (size_t i = 0; i < rows; i++)
    {
        array[i] = limdy_memory_pool_alloc_from(pool, cols * sizeof(float));
        if (!array[i])
        {
            for (size_t j = 0; j < i; j++)
            {
                limdy_memory_pool_free_to(pool, array[j]);
            }
            limdy_memory_pool_free_to(pool, array);
            LOG_ERROR(ERROR_MEMORY_POOL_ALLOC_FAILED, "Failed to allocate memory for 2D float array row");
            return NULL;
        }
    }
    return array;
}

// Helper function to free 2D float array using memory pool
static void free_2d_float_array(LimdyMemoryPool *pool, float **array, size_t rows)
{
    if (!array)
        return;
    for (size_t i = 0; i < rows; i++)
    {
        limdy_memory_pool_free_to(pool, array[i]);
    }
    limdy_memory_pool_free_to(pool, array);
}

// Translator functions
Translator *translator_create(TranslationService *service)
{
    CHECK_NULL(service, NULL);

    Translator *translator = limdy_memory_pool_alloc(sizeof(Translator));
    CHECK_NULL(translator, NULL);

    translator->service = service;

    if (pthread_mutex_init(&translator->mutex, NULL) != 0)
    {
        LOG_ERROR(ERROR_THREAD_INIT, "Failed to initialize translator mutex");
        limdy_memory_pool_free(translator);
        return NULL;
    }

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

void translator_destroy(Translator *translator)
{
    if (translator)
    {
        pthread_mutex_destroy(&translator->mutex);
        limdy_memory_pool_destroy(translator->pool);
        limdy_memory_pool_free(translator);
    }
}

ErrorCode translator_translate(Translator *translator, const char *text, const char *source_lang, const char *target_lang, TranslationResult *result)
{
    CHECK_NULL(translator, ERROR_NULL_POINTER);
    CHECK_NULL(text, ERROR_NULL_POINTER);
    CHECK_NULL(source_lang, ERROR_NULL_POINTER);
    CHECK_NULL(target_lang, ERROR_NULL_POINTER);
    CHECK_NULL(result, ERROR_NULL_POINTER);

    MUTEX_LOCK(&translator->mutex);

    memset(result, 0, sizeof(TranslationResult));

    ErrorCode error = translator->service->translate(text, source_lang, target_lang, &result->translated_text);
    if (error != ERROR_SUCCESS)
    {
        LOG_ERROR(error, "Translation failed");
        MUTEX_UNLOCK(&translator->mutex);
        return error;
    }

    error = translator->service->get_attention_matrix(text, result->translated_text, &result->attention_matrix, &result->rows, &result->cols);
    if (error != ERROR_SUCCESS)
    {
        LOG_ERROR(error, "Failed to get attention matrix");
        limdy_memory_pool_free_to(translator->pool, result->translated_text);
        result->translated_text = NULL;
        MUTEX_UNLOCK(&translator->mutex);
        return error;
    }

    MUTEX_UNLOCK(&translator->mutex);
    return ERROR_SUCCESS;
}

// Aligner functions
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

void aligner_destroy(Aligner *aligner)
{
    if (aligner)
    {
        pthread_mutex_destroy(&aligner->mutex);
        limdy_memory_pool_destroy(aligner->pool);
        limdy_memory_pool_free(aligner);
    }
}

ErrorCode aligner_align(Aligner *aligner, const char *source_text, const char *target_text, float **attention_matrix, size_t rows, size_t cols, char ***aligned_text, size_t *aligned_size)
{
    CHECK_NULL(aligner, ERROR_NULL_POINTER);
    CHECK_NULL(source_text, ERROR_NULL_POINTER);
    CHECK_NULL(target_text, ERROR_NULL_POINTER);
    CHECK_NULL(attention_matrix, ERROR_NULL_POINTER);
    CHECK_NULL(aligned_text, ERROR_NULL_POINTER);
    CHECK_NULL(aligned_size, ERROR_NULL_POINTER);

    MUTEX_LOCK(&aligner->mutex);

    char **source_tokens = NULL;
    size_t source_token_count = 0;
    char **target_tokens = NULL;
    size_t target_token_count = 0;
    int *alignment = NULL;
    size_t alignment_size = 0;
    ErrorCode error = ERROR_SUCCESS;

    // Tokenize source and target text
    error = aligner->renderer->tokenize(source_text, &source_tokens, &source_token_count);
    if (error != ERROR_SUCCESS)
    {
        LOG_ERROR(error, "Failed to tokenize source text");
        goto cleanup;
    }

    error = aligner->renderer->tokenize(target_text, &target_tokens, &target_token_count);
    if (error != ERROR_SUCCESS)
    {
        LOG_ERROR(error, "Failed to tokenize target text");
        goto cleanup;
    }

    // Align tokens
    error = aligner->service->align_tokens((const char **)source_tokens, source_token_count,
                                           (const char **)target_tokens, target_token_count,
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
        LOG_ERROR(ERROR_MEMORY_POOL_ALLOC_FAILED, "Failed to allocate memory for aligned text");
        error = ERROR_MEMORY_POOL_ALLOC_FAILED;
        goto cleanup;
    }

    for (size_t i = 0; i < alignment_size; i++)
    {
        size_t len = strlen(source_tokens[i]) + strlen(target_tokens[alignment[i]]) + 4; // 4 for "[", "]", " ", and null terminator
        (*aligned_text)[i] = limdy_memory_pool_alloc_from(aligner->pool, len * sizeof(char));
        if (!(*aligned_text)[i])
        {
            LOG_ERROR(ERROR_MEMORY_POOL_ALLOC_FAILED, "Failed to allocate memory for aligned text entry");
            error = ERROR_MEMORY_POOL_ALLOC_FAILED;
            goto cleanup;
        }
        snprintf((*aligned_text)[i], len, "[%s] [%s]", source_tokens[i], target_tokens[alignment[i]]);
    }

    *aligned_size = alignment_size;

cleanup:
    if (source_tokens)
        aligner->renderer->free_tokens(source_tokens, source_token_count);
    if (target_tokens)
        aligner->renderer->free_tokens(target_tokens, target_token_count);
    limdy_memory_pool_free_to(aligner->pool, alignment);

    if (error != ERROR_SUCCESS && *aligned_text)
    {
        free_aligned_text(*aligned_text, alignment_size);
        *aligned_text = NULL;
        *aligned_size = 0;
    }

    MUTEX_UNLOCK(&aligner->mutex);
    return error;
}

// TranslatorAligner functions
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

    ErrorCode error = limdy_memory_pool_create(LIMDY_SMALL_POOL_SIZE, &ta->pool);
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
// Helper functions
void free_translation_result(TranslationResult *result)
{
    if (result)
    {
        limdy_memory_pool_free(result->translated_text);
        free_2d_float_array(result->attention_matrix, result->rows);
        memset(result, 0, sizeof(TranslationResult));
    }
}
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