/**
 * @file error_handler.h
 * @brief Error handling system for the Limdy project.
 *
 * This file defines the error handling system used throughout the Limdy project.
 * It provides a centralized way to define error codes, log errors, and handle
 * them consistently across the application.
 *
 * @author Limdy Development Team
 * @date 2024-08-22
 */

#ifndef LIMDY_UTILS_ERROR_HANDLER_H
#define LIMDY_UTILS_ERROR_HANDLER_H

#include <stddef.h>
#include <stdarg.h>

/**
 * @brief Enumeration of error severity levels.
 */
typedef enum
{
    ERROR_LEVEL_DEBUG,   /**< Debug-level message */
    ERROR_LEVEL_INFO,    /**< Informational message */
    ERROR_LEVEL_WARNING, /**< Warning message */
    ERROR_LEVEL_ERROR,   /**< Error message */
    ERROR_LEVEL_FATAL    /**< Fatal error message */
} ErrorLevel;

/**
 * @brief Enumeration of error codes used throughout the Limdy project.
 */
typedef enum
{
    ERROR_SUCCESS = 0,              /**< Operation completed successfully */
    ERROR_NULL_POINTER,             /**< Null pointer error */
    ERROR_INVALID_ARGUMENT,         /**< Invalid argument provided */
    ERROR_MEMORY_ALLOCATION,        /**< Memory allocation failure */
    ERROR_FILE_IO,                  /**< File input/output error */
    ERROR_NETWORK,                  /**< Network-related error */
    ERROR_UNKNOWN,                  /**< Unknown error */
    ERROR_THREAD_LOCK,              /**< Thread lock error */
    ERROR_THREAD_UNLOCK,            /**< Thread unlock error */
    ERROR_THREAD_INIT,              /**< Thread initialization error */
    ERROR_MEMORY_POOL_INIT_FAILED,  /**< Memory pool initialization failure */
    ERROR_MEMORY_POOL_ALLOC_FAILED, /**< Memory pool allocation failure */
    ERROR_MEMORY_POOL_INVALID_FREE, /**< Invalid memory free operation */
    ERROR_MEMORY_POOL_FULL,         /**< Memory pool is full */
    ERROR_MEMORY_POOL_INVALID_POOL, /**< Invalid memory pool */
    ERROR_CUSTOM_BASE = 1000        /**< Base for component-specific error codes */
} ErrorCode;

/**
 * @brief Structure to hold context information about an error.
 */
typedef struct
{
    ErrorCode code;       /**< The error code */
    ErrorLevel level;     /**< The error severity level */
    const char *file;     /**< The file where the error occurred */
    int line;             /**< The line number where the error occurred */
    const char *function; /**< The function where the error occurred */
    char message[256];    /**< The error message */
} ErrorContext;

/**
 * @brief Function pointer type for custom error handlers.
 */
typedef void (*ErrorHandler)(const ErrorContext *context);

/**
 * @brief Initialize the error handling system.
 *
 * This function should be called once at the start of the program.
 */
void error_init(void);

/**
 * @brief Clean up the error handling system.
 *
 * This function should be called once at the end of the program.
 */
void error_cleanup(void);

/**
 * @brief Set a custom error handler.
 *
 * @param handler Pointer to the custom error handler function.
 */
void error_set_handler(ErrorHandler handler);

/**
 * @brief Set the minimum error level for logging.
 *
 * @param level The minimum error level to be logged.
 */
void error_set_min_level(ErrorLevel level);

/**
 * @brief Log an error.
 *
 * @param code The error code.
 * @param level The error severity level.
 * @param file The file where the error occurred.
 * @param line The line number where the error occurred.
 * @param function The function where the error occurred.
 * @param format The format string for the error message.
 * @param ... Additional arguments for the format string.
 */
void error_log(ErrorCode code, ErrorLevel level, const char *file, int line, const char *function, const char *format, ...);

/**
 * @brief Get the last error context.
 *
 * @return Pointer to the last error context.
 */
const ErrorContext *error_get_last(void);

/**
 * @brief Clear the last error.
 */
void error_clear(void);

/**
 * @brief Convenience macro for logging debug messages.
 */
#define LOG_DEBUG(code, ...) error_log(code, ERROR_LEVEL_DEBUG, __FILE__, __LINE__, __func__, __VA_ARGS__)

/**
 * @brief Convenience macro for logging info messages.
 */
#define LOG_INFO(code, ...) error_log(code, ERROR_LEVEL_INFO, __FILE__, __LINE__, __func__, __VA_ARGS__)

/**
 * @brief Convenience macro for logging warning messages.
 */
#define LOG_WARNING(code, ...) error_log(code, ERROR_LEVEL_WARNING, __FILE__, __LINE__, __func__, __VA_ARGS__)

/**
 * @brief Convenience macro for logging error messages.
 */
#define LOG_ERROR(code, ...) error_log(code, ERROR_LEVEL_ERROR, __FILE__, __LINE__, __func__, __VA_ARGS__)

/**
 * @brief Convenience macro for logging fatal error messages.
 */
#define LOG_FATAL(code, ...) error_log(code, ERROR_LEVEL_FATAL, __FILE__, __LINE__, __func__, __VA_ARGS__)

/**
 * @brief Macro for checking and logging errors.
 *
 * @param expression The expression to check.
 */
#define CHECK_ERROR(expression)                                            \
    do                                                                     \
    {                                                                      \
        if (!(expression))                                                 \
        {                                                                  \
            LOG_ERROR(ERROR_UNKNOWN, "Assertion failed: %s", #expression); \
            return ERROR_UNKNOWN;                                          \
        }                                                                  \
    } while (0)

#endif // LIMDY_UTILS_ERROR_HANDLER_H