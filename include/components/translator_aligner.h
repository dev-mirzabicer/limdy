#ifndef LIMDY_COMPONENTS_TRANSLATOR_ALIGNER_H
#define LIMDY_COMPONENTS_TRANSLATOR_ALIGNER_H

#include <stddef.h>
#include <pthread.h>
#include "error_handler.h"
#include "renderer.h"
#include "memory_pool.h"

typedef struct
{
    char *translated_text;
    float **attention_matrix;
    size_t rows;
    size_t cols;
} TranslationResult;

typedef struct
{
    ErrorCode (*translate)(const char *text, const char *source_lang, const char *target_lang, char **translated_text);
    ErrorCode (*get_attention_matrix)(const char *source_text, const char *target_text, float ***attention_matrix, size_t *rows, size_t *cols);
} TranslationService;

typedef struct
{
    ErrorCode (*align_tokens)(const char **source_tokens, size_t source_token_count,
                              const char **target_tokens, size_t target_token_count,
                              float **attention_matrix, size_t rows, size_t cols,
                              int **alignment, size_t *alignment_size);
} AlignmentService;

typedef struct
{
    TranslationService *service;
    pthread_mutex_t mutex;
    LimdyMemoryPool *pool;
} Translator;

typedef struct
{
    AlignmentService *service;
    Renderer *renderer;
    pthread_mutex_t mutex;
    LimdyMemoryPool *pool;
} Aligner;

typedef struct
{
    Translator *translator;
    Aligner *aligner;
    pthread_mutex_t mutex;
    LimdyMemoryPool *pool;
} TranslatorAligner;

// Function prototypes
Translator *translator_create(TranslationService *service);
void translator_destroy(Translator *translator);
ErrorCode translator_translate(Translator *translator, const char *text, const char *source_lang, const char *target_lang, TranslationResult *result);

Aligner *aligner_create(AlignmentService *service, Renderer *renderer);
void aligner_destroy(Aligner *aligner);
ErrorCode aligner_align(Aligner *aligner, const char *source_text, const char *target_text, float **attention_matrix, size_t rows, size_t cols, char ***aligned_text, size_t *aligned_size);

TranslatorAligner *translator_aligner_create(TranslationService *trans_service, AlignmentService *align_service, Renderer *renderer);
void translator_aligner_destroy(TranslatorAligner *ta);
ErrorCode translator_aligner_process(TranslatorAligner *ta, const char *text, const char *source_lang, const char *target_lang, char ***aligned_text, size_t *aligned_size);

void free_translation_result(TranslationResult *result);
void free_aligned_text(char **aligned_text, size_t size);

#endif // LIMDY_COMPONENTS_TRANSLATOR_ALIGNER_H