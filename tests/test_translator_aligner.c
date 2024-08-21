#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "translator_aligner.h"
#include "error_handler.h"

// Mock TranslationService
ErrorCode mock_translate(const char *text, const char *source_lang, const char *target_lang, char **translated_text)
{
    *translated_text = strdup("Mocked translation");
    return ERROR_SUCCESS;
}

ErrorCode mock_get_attention_matrix(const char *source_text, const char *translated_text, float ***matrix, size_t *rows, size_t *cols)
{
    *rows = 2;
    *cols = 2;
    *matrix = malloc((*rows) * sizeof(float *));
    for (size_t i = 0; i < *rows; i++)
    {
        (*matrix)[i] = malloc((*cols) * sizeof(float));
        for (size_t j = 0; j < *cols; j++)
        {
            (*matrix)[i][j] = 0.5;
        }
    }
    return ERROR_SUCCESS;
}

TranslationService mock_translation_service = {
    .translate = mock_translate,
    .get_attention_matrix = mock_get_attention_matrix};

// Mock AlignmentService
ErrorCode mock_align_tokens(const char **source_tokens, size_t source_count,
                            const char **target_tokens, size_t target_count,
                            float **attention_matrix, size_t rows, size_t cols,
                            int **alignment, size_t *alignment_size)
{
    *alignment_size = 2;
    *alignment = malloc((*alignment_size) * sizeof(int));
    (*alignment)[0] = 0;
    (*alignment)[1] = 1;
    return ERROR_SUCCESS;
}

AlignmentService mock_alignment_service = {
    .align_tokens = mock_align_tokens};

// Mock Renderer
ErrorCode mock_tokenize(const char *text, char ***tokens, size_t *token_count)
{
    *token_count = 2;
    *tokens = malloc((*token_count) * sizeof(char *));
    (*tokens)[0] = strdup("Token1");
    (*tokens)[1] = strdup("Token2");
    return ERROR_SUCCESS;
}

void mock_free_tokens(char **tokens, size_t token_count)
{
    for (size_t i = 0; i < token_count; i++)
    {
        free(tokens[i]);
    }
    free(tokens);
}

Renderer mock_renderer = {
    .tokenize = mock_tokenize,
    .free_tokens = mock_free_tokens};

// Custom error handler for testing
static ErrorContext last_error;
void test_error_handler(const ErrorContext *context)
{
    memcpy(&last_error, context, sizeof(ErrorContext));
}

// Test functions
void test_translator_create()
{
    Translator *translator = translator_create(&mock_translation_service);
    assert(translator != NULL);
    assert(translator->service == &mock_translation_service);
    translator_destroy(translator);
    printf("test_translator_create() passed.\n");
}

void test_translator_translate()
{
    Translator *translator = translator_create(&mock_translation_service);
    TranslationResult result = {0};
    ErrorCode error = translator_translate(translator, "Hello", "en", "fr", &result);
    assert(error == ERROR_SUCCESS);
    assert(strcmp(result.translated_text, "Mocked translation") == 0);
    assert(result.rows == 2);
    assert(result.cols == 2);
    assert(result.attention_matrix != NULL);
    free_translation_result(&result);
    translator_destroy(translator);
    printf("test_translator_translate() passed.\n");
}

void test_aligner_create()
{
    Aligner *aligner = aligner_create(&mock_alignment_service, &mock_renderer);
    assert(aligner != NULL);
    assert(aligner->service == &mock_alignment_service);
    assert(aligner->renderer == &mock_renderer);
    aligner_destroy(aligner);
    printf("test_aligner_create() passed.\n");
}

void test_aligner_align()
{
    Aligner *aligner = aligner_create(&mock_alignment_service, &mock_renderer);
    char **aligned_text;
    size_t aligned_size;
    float **attention_matrix = malloc(2 * sizeof(float *));
    for (int i = 0; i < 2; i++)
    {
        attention_matrix[i] = malloc(2 * sizeof(float));
        attention_matrix[i][0] = attention_matrix[i][1] = 0.5;
    }
    ErrorCode error = aligner_align(aligner, "Source", "Target", attention_matrix, 2, 2, &aligned_text, &aligned_size);
    assert(error == ERROR_SUCCESS);
    assert(aligned_size == 2);
    assert(strcmp(aligned_text[0], "[Token1] [Token1]") == 0);
    assert(strcmp(aligned_text[1], "[Token2] [Token2]") == 0);
    free_aligned_text(aligned_text, aligned_size);
    for (int i = 0; i < 2; i++)
    {
        free(attention_matrix[i]);
    }
    free(attention_matrix);
    aligner_destroy(aligner);
    printf("test_aligner_align() passed.\n");
}

void test_translator_aligner_create()
{
    TranslatorAligner *ta = translator_aligner_create(&mock_translation_service, &mock_alignment_service, &mock_renderer);
    assert(ta != NULL);
    assert(ta->translator != NULL);
    assert(ta->aligner != NULL);
    translator_aligner_destroy(ta);
    printf("test_translator_aligner_create() passed.\n");
}

void test_translator_aligner_process()
{
    TranslatorAligner *ta = translator_aligner_create(&mock_translation_service, &mock_alignment_service, &mock_renderer);
    char **aligned_text;
    size_t aligned_size;
    ErrorCode error = translator_aligner_process(ta, "Hello", "en", "fr", &aligned_text, &aligned_size);
    assert(error == ERROR_SUCCESS);
    assert(aligned_size == 2);
    assert(strcmp(aligned_text[0], "[Token1] [Token1]") == 0);
    assert(strcmp(aligned_text[1], "[Token2] [Token2]") == 0);
    free_aligned_text(aligned_text, aligned_size);
    translator_aligner_destroy(ta);
    printf("test_translator_aligner_process() passed.\n");
}

void test_error_handling()
{
    error_set_handler(test_error_handler);

    // Test NULL pointer error
    Translator *translator = translator_create(NULL);
    assert(translator == NULL);
    assert(last_error.code == ERROR_NULL_POINTER);

    // Test invalid input error
    TranslatorAligner *ta = translator_aligner_create(&mock_translation_service, &mock_alignment_service, &mock_renderer);
    char **aligned_text;
    size_t aligned_size;
    ErrorCode error = translator_aligner_process(ta, NULL, "en", "fr", &aligned_text, &aligned_size);
    assert(error == ERROR_NULL_POINTER);
    assert(last_error.code == ERROR_NULL_POINTER);

    translator_aligner_destroy(ta);
    error_set_handler(NULL);
    printf("test_error_handling() passed.\n");
}

int main()
{
    error_init();

    test_translator_create();
    test_translator_translate();
    test_aligner_create();
    test_aligner_align();
    test_translator_aligner_create();
    test_translator_aligner_process();
    test_error_handling();

    error_cleanup();

    printf("All tests passed successfully.\n");
    return 0;
}