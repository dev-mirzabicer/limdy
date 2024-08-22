#ifndef LIMDY_UTILS_ERROR_HANDLER_H
#define LIMDY_UTILS_ERROR_HANDLER_H

#include <stddef.h>
#include <stdarg.h>

// Error levels
typedef enum
{
    ERROR_LEVEL_DEBUG,
    ERROR_LEVEL_INFO,
    ERROR_LEVEL_WARNING,
    ERROR_LEVEL_ERROR,
    ERROR_LEVEL_FATAL
} ErrorLevel;

// Error codes
typedef enum
{
    ERROR_SUCCESS = 0,
    ERROR_NULL_POINTER,
    ERROR_INVALID_ARGUMENT,
    ERROR_MEMORY_ALLOCATION,
    ERROR_FILE_IO,
    ERROR_NETWORK,
    ERROR_UNKNOWN,
    // Thread-related error codes
    ERROR_THREAD_LOCK,
    ERROR_THREAD_UNLOCK,
    ERROR_THREAD_INIT,
    // Memory pool error codes
    ERROR_MEMORY_POOL_INIT_FAILED,
    ERROR_MEMORY_POOL_ALLOC_FAILED,
    ERROR_MEMORY_POOL_INVALID_FREE,
    ERROR_MEMORY_POOL_FULL,
    ERROR_MEMORY_POOL_INVALID_POOL,
    // Add more error codes as needed
    ERROR_CUSTOM_BASE = 1000 // Base for component-specific error codes
} ErrorCode;

// Error context structure
typedef struct
{
    ErrorCode code;
    ErrorLevel level;
    const char *file;
    int line;
    const char *function;
    char message[256];
} ErrorContext;

// Function pointer type for custom error handlers
typedef void (*ErrorHandler)(const ErrorContext *context);

// Initialize the error handling system
void error_init(void);

// Clean up the error handling system
void error_cleanup(void);

// Set a custom error handler
void error_set_handler(ErrorHandler handler);

// Set the minimum error level for logging
void error_set_min_level(ErrorLevel level);

// Log an error
void error_log(ErrorCode code, ErrorLevel level, const char *file, int line, const char *function, const char *format, ...);

// Get the last error context
const ErrorContext *error_get_last(void);

// Clear the last error
void error_clear(void);

// Convenience macros for logging
#define LOG_DEBUG(code, ...) error_log(code, ERROR_LEVEL_DEBUG, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_INFO(code, ...) error_log(code, ERROR_LEVEL_INFO, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_WARNING(code, ...) error_log(code, ERROR_LEVEL_WARNING, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_ERROR(code, ...) error_log(code, ERROR_LEVEL_ERROR, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_FATAL(code, ...) error_log(code, ERROR_LEVEL_FATAL, __FILE__, __LINE__, __func__, __VA_ARGS__)

// Error checking macro
#define CHECK_ERROR(expression)                                            \
    do                                                                     \
    {                                                                      \
        if (!(expression))                                                 \
        {                                                                  \
            LOG_ERROR(ERROR_UNKNOWN, "Assertion failed: %s", #expression); \
            return ERROR_UNKNOWN;                                          \
        }                                                                  \
    } while (0)

#endif // ERROR_HANDLER_H