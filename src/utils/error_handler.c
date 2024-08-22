/**
 * @file error_handler.c
 * @brief Implementation of the error handling system for the Limdy project.
 *
 * This file implements the error handling system defined in error_handler.h.
 * It provides functions for logging errors, managing error contexts, and
 * handling errors consistently across the application.
 *
 * @author Mirza Bicer
 * @date 2024-08-22
 */

#include "utils/error_handler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

#define MAX_ERROR_QUEUE_SIZE 100

/**
 * @brief Thread-local storage for error context.
 */
static __thread ErrorContext tls_error_context;

/**
 * @brief Global error handler function pointer.
 */
static ErrorHandler global_error_handler = NULL;

/**
 * @brief Minimum error level for logging.
 */
static ErrorLevel min_error_level = ERROR_LEVEL_DEBUG;

/**
 * @brief Mutex for thread-safe operations.
 */
static pthread_mutex_t error_mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * @brief Circular buffer for error history.
 */
static ErrorContext error_history[MAX_ERROR_QUEUE_SIZE];

/**
 * @brief Start index of the error history circular buffer.
 */
static int error_history_start = 0;

/**
 * @brief Number of errors in the error history.
 */
static int error_history_count = 0;

void error_init(void)
{
    pthread_mutex_init(&error_mutex, NULL);
}

void error_cleanup(void)
{
    pthread_mutex_destroy(&error_mutex);
}

void error_set_handler(ErrorHandler handler)
{
    pthread_mutex_lock(&error_mutex);
    global_error_handler = handler;
    pthread_mutex_unlock(&error_mutex);
}

void error_set_min_level(ErrorLevel level)
{
    pthread_mutex_lock(&error_mutex);
    min_error_level = level;
    pthread_mutex_unlock(&error_mutex);
}

/**
 * @brief Add an error context to the error history.
 *
 * @param context Pointer to the error context to be added.
 */
static void add_to_error_history(const ErrorContext *context)
{
    pthread_mutex_lock(&error_mutex);

    if (error_history_count < MAX_ERROR_QUEUE_SIZE)
    {
        error_history_count++;
    }
    else
    {
        error_history_start = (error_history_start + 1) % MAX_ERROR_QUEUE_SIZE;
    }

    int index = (error_history_start + error_history_count - 1) % MAX_ERROR_QUEUE_SIZE;
    memcpy(&error_history[index], context, sizeof(ErrorContext));

    pthread_mutex_unlock(&error_mutex);
}

void error_log(ErrorCode code, ErrorLevel level, const char *file, int line, const char *function, const char *format, ...)
{
    if (level < min_error_level)
    {
        return;
    }

    va_list args;
    va_start(args, format);

    ErrorContext context;
    context.code = code;
    context.level = level;
    context.file = file;
    context.line = line;
    context.function = function;
    vsnprintf(context.message, sizeof(context.message), format, args);

    va_end(args);

    // Store in thread-local storage
    memcpy(&tls_error_context, &context, sizeof(ErrorContext));

    // Add to error history
    add_to_error_history(&context);

    // Call global error handler if set
    if (global_error_handler)
    {
        global_error_handler(&context);
    }
    else
    {
        // Default error handling: print to stderr
        fprintf(stderr, "[%s:%d] %s: %s\n", file, line, function, context.message);
    }
}

const ErrorContext *error_get_last(void)
{
    return &tls_error_context;
}

void error_clear(void)
{
    memset(&tls_error_context, 0, sizeof(ErrorContext));
}

/**
 * @brief Get the string representation of an error level.
 *
 * @param level The error level.
 * @return The string representation of the error level.
 */
static const char *get_error_level_string(ErrorLevel level)
{
    switch (level)
    {
    case ERROR_LEVEL_DEBUG:
        return "DEBUG";
    case ERROR_LEVEL_INFO:
        return "INFO";
    case ERROR_LEVEL_WARNING:
        return "WARNING";
    case ERROR_LEVEL_ERROR:
        return "ERROR";
    case ERROR_LEVEL_FATAL:
        return "FATAL";
    default:
        return "UNKNOWN";
    }
}

/**
 * @brief Get the string representation of an error code.
 *
 * @param code The error code.
 * @return The string representation of the error code.
 */
static const char *get_error_code_string(ErrorCode code)
{
    switch (code)
    {
    case ERROR_SUCCESS:
        return "SUCCESS";
    case ERROR_NULL_POINTER:
        return "NULL_POINTER";
    case ERROR_INVALID_ARGUMENT:
        return "INVALID_ARGUMENT";
    case ERROR_MEMORY_ALLOCATION:
        return "MEMORY_ALLOCATION";
    case ERROR_FILE_IO:
        return "FILE_IO";
    case ERROR_NETWORK:
        return "NETWORK";
    case ERROR_THREAD_LOCK:
        return "THREAD_LOCK";
    case ERROR_THREAD_UNLOCK:
        return "THREAD_UNLOCK";
    case ERROR_THREAD_INIT:
        return "THREAD_INIT";
    case ERROR_UNKNOWN:
        return "UNKNOWN";
    case ERROR_MEMORY_POOL_INIT_FAILED:
        return "MEMORY_POOL_INIT_FAILED";
    case ERROR_MEMORY_POOL_ALLOC_FAILED:
        return "MEMORY_POOL_ALLOC_FAILED";
    case ERROR_MEMORY_POOL_INVALID_FREE:
        return "MEMORY_POOL_INVALID_FREE";
    case ERROR_MEMORY_POOL_FULL:
        return "MEMORY_POOL_FULL";
    case ERROR_MEMORY_POOL_INVALID_POOL:
        return "MEMORY_POOL_INVALID_POOL";
    default:
        return "CUSTOM_ERROR";
    }
}

/**
 * @brief Default error handler that logs to a file.
 *
 * @param context Pointer to the error context.
 */
void default_error_handler(const ErrorContext *context)
{
    FILE *log_file = fopen("error.log", "a");
    if (log_file == NULL)
    {
        fprintf(stderr, "Failed to open error log file\n");
        return;
    }

    time_t now;
    time(&now);
    char timestamp[26];
    ctime_r(&now, timestamp);
    timestamp[24] = '\0'; // Remove newline

    fprintf(log_file, "[%s] [%s] [%s:%d] %s: (Error Code: %d) %s\n",
            timestamp,
            get_error_level_string(context->level),
            context->file,
            context->line,
            context->function,
            context->code,
            context->message);

    fclose(log_file);
}

/**
 * @brief Set the default error handler.
 */
void error_set_default_handler(void)
{
    error_set_handler(default_error_handler);
}