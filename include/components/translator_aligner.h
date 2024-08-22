/**
 * @file translator_aligner.h
 * @brief Translation and alignment system for the Limdy project.
 *
 * This file defines the structures and functions for translating text and
 * aligning the translated text with the original. It provides an interface
 * for translation services, alignment services, and combines them into a
 * unified translator-aligner system.
 *
 * @author Mirza Bicer
 * @date 2024-08-22
 */

#ifndef LIMDY_COMPONENTS_TRANSLATOR_ALIGNER_H
#define LIMDY_COMPONENTS_TRANSLATOR_ALIGNER_H

#include <stddef.h>
#include <pthread.h>
#include "error_handler.h"
#include "renderer.h"
#include "memory_pool.h"

/**
 * @brief Structure to hold the result of a translation operation.
 */
typedef struct
{
    char *translated_text;    /**< The translated text */
    float **attention_matrix; /**< Attention matrix (from the translator) for alignment */
    size_t rows;              /**< Number of rows in the attention matrix */
    size_t cols;              /**< Number of columns in the attention matrix */
} TranslationResult;

/**
 * @brief Interface for translation services.
 */
typedef struct
{
    /**
     * @brief Function pointer for text translation.
     *
     * @param text The text to translate.
     * @param source_lang The source language.
     * @param target_lang The target language.
     * @param translated_text Pointer to store the translated text.
     * @return ErrorCode indicating success or failure.
     */
    ErrorCode (*translate)(const char *text, const char *source_lang, const char *target_lang, char **translated_text);

    /**
     * @brief Function pointer for getting the attention matrix.
     *
     * @param source_text The source text.
     * @param target_text The target (translated) text.
     * @param attention_matrix Pointer to store the attention matrix.
     * @param rows Pointer to store the number of rows in the matrix.
     * @param cols Pointer to store the number of columns in the matrix.
     * @return ErrorCode indicating success or failure.
     */
    ErrorCode (*get_attention_matrix)(const char *source_text, const char *target_text, float ***attention_matrix, size_t *rows, size_t *cols);
} TranslationService;

/**
 * @brief Interface for alignment services.
 */
typedef struct
{
    /**
     * @brief Function pointer for aligning tokens.
     *
     * @param source_tokens Array of source language tokens.
     * @param source_token_count Number of source tokens.
     * @param target_tokens Array of target language tokens.
     * @param target_token_count Number of target tokens.
     * @param attention_matrix The attention matrix from the translator.
     * @param rows Number of rows in the attention matrix.
     * @param cols Number of columns in the attention matrix.
     * @param alignment Pointer to store the alignment result.
     * @param alignment_size Pointer to store the size of the alignment.
     * @return ErrorCode indicating success or failure.
     */
    ErrorCode (*align_tokens)(const char **source_tokens, size_t source_token_count,
                              const char **target_tokens, size_t target_token_count,
                              float **attention_matrix, size_t rows, size_t cols,
                              int **alignment, size_t *alignment_size);
} AlignmentService;

/**
 * @brief Structure representing a translator.
 */
typedef struct
{
    TranslationService *service; /**< The translation service */
    pthread_mutex_t mutex;       /**< Mutex for thread-safe operations */
    LimdyMemoryPool *pool;       /**< Memory pool for allocations */
} Translator;

/**
 * @brief Structure representing an aligner.
 */
typedef struct
{
    AlignmentService *service; /**< The alignment service */
    Renderer *renderer;        /**< The renderer for tokenization & processing */
    pthread_mutex_t mutex;     /**< Mutex for thread-safe operations */
    LimdyMemoryPool *pool;     /**< Memory pool for allocations */
} Aligner;

/**
 * @brief Structure combining translator and aligner.
 */
typedef struct
{
    Translator *translator; /**< The translator */
    Aligner *aligner;       /**< The aligner */
    pthread_mutex_t mutex;  /**< Mutex for thread-safe operations */
    LimdyMemoryPool *pool;  /**< Memory pool for allocations */
} TranslatorAligner;

/**
 * @brief Create a new translator.
 *
 * @param service The translation service to use.
 * @return Pointer to the created Translator, or NULL on failure.
 */
Translator *translator_create(TranslationService *service);

/**
 * @brief Destroy a translator and free its resources.
 *
 * @param translator The translator to destroy.
 */
void translator_destroy(Translator *translator);

/**
 * @brief Perform a translation operation.
 *
 * @param translator The translator to use.
 * @param text The text to translate.
 * @param source_lang The source language.
 * @param target_lang The target language.
 * @param result Pointer to store the translation result.
 * @return ErrorCode indicating success or failure.
 */
ErrorCode translator_translate(Translator *translator, const char *text, const char *source_lang, const char *target_lang, TranslationResult *result);

/**
 * @brief Create a new aligner.
 *
 * @param service The alignment service to use.
 * @param renderer The renderer to use for tokenization.
 * @return Pointer to the created Aligner, or NULL on failure.
 */
Aligner *aligner_create(AlignmentService *service, Renderer *renderer);

/**
 * @brief Destroy an aligner and free its resources.
 *
 * @param aligner The aligner to destroy.
 */
void aligner_destroy(Aligner *aligner);

/**
 * @brief Perform an alignment operation.
 *
 * @param aligner The aligner to use.
 * @param source_text The source text.
 * @param target_text The target (translated) text.
 * @param attention_matrix The attention matrix from the translator.
 * @param rows Number of rows in the attention matrix.
 * @param cols Number of columns in the attention matrix.
 * @param aligned_text Pointer to store the aligned text.
 * @param aligned_size Pointer to store the size of the aligned text.
 * @return ErrorCode indicating success or failure.
 */
ErrorCode aligner_align(Aligner *aligner, const char *source_text, const char *target_text, float **attention_matrix, size_t rows, size_t cols, char ***aligned_text, size_t *aligned_size);

/**
 * @brief Create a new translator-aligner.
 *
 * @param trans_service The translation service to use.
 * @param align_service The alignment service to use.
 * @param renderer The renderer to use for tokenization.
 * @return Pointer to the created TranslatorAligner, or NULL on failure.
 */
TranslatorAligner *translator_aligner_create(TranslationService *trans_service, AlignmentService *align_service, Renderer *renderer);

/**
 * @brief Destroy a translator-aligner and free its resources.
 *
 * @param ta The translator-aligner to destroy.
 */
void translator_aligner_destroy(TranslatorAligner *ta);

/**
 * @brief Perform a combined translation and alignment operation.
 *
 * @param ta The translator-aligner to use.
 * @param text The text to translate and align.
 * @param source_lang The source language.
 * @param target_lang The target language.
 * @param aligned_text Pointer to store the aligned text.
 * @param aligned_size Pointer to store the size of the aligned text.
 * @return ErrorCode indicating success or failure.
 */
ErrorCode translator_aligner_process(TranslatorAligner *ta, const char *text, const char *source_lang, const char *target_lang, char ***aligned_text, size_t *aligned_size);

/**
 * @brief Free the resources of a translation result.
 *
 * @param result The translation result to free.
 */
void free_translation_result(TranslationResult *result);

/**
 * @brief Free the resources of aligned text.
 *
 * @param aligned_text The aligned text to free.
 * @param size The size of the aligned text.
 */
void free_aligned_text(char **aligned_text, size_t size);

#endif // LIMDY_COMPONENTS_TRANSLATOR_ALIGNER_H