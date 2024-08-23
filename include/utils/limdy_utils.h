/**
 * @file limdy_utils.h
 * @brief Utility macros for the Limdy project.
 *
 * This file contains various utility macros that are used throughout the Limdy project.
 * These macros provide common functionality for error checking, mutex operations,
 * and other frequently used patterns.
 *
 * @author Mirza Bicer
 * @date 2024-08-22
 */

#ifndef LIMDY_UTILS_H
#define LIMDY_UTILS_H

#include "error_handler.h"
#include <pthread.h>

/**
 * @brief Checks if a pointer is NULL and logs an error if it is.
 *
 * This macro checks if the given pointer is NULL. If it is, it logs an error
 * with the provided error code and returns that error code.
 *
 * @param ptr The pointer to check.
 * @param error_code The error code to return if the pointer is NULL.
 */
#define CHECK_NULL(ptr, error_code)                \
    do                                             \
    {                                              \
        if ((ptr) == NULL)                         \
        {                                          \
            LOG_ERROR(error_code, "Null pointer"); \
            return error_code;                     \
        }                                          \
    } while (0)

/**
 * @brief Attempts to lock a mutex and logs an error if it fails.
 *
 * This macro tries to lock the given mutex. If the lock operation fails,
 * it logs an error and returns an error code.
 *
 * @param mutex The mutex to lock.
 */
#define MUTEX_LOCK(mutex)                                         \
    do                                                            \
    {                                                             \
        if (pthread_mutex_lock(mutex) != 0)                       \
        {                                                         \
            LOG_ERROR(ERROR_THREAD_LOCK, "Failed to lock mutex"); \
            return ERROR_THREAD_LOCK;                             \
        }                                                         \
    } while (0)

/**
 * @brief Attempts to unlock a mutex and logs an error if it fails.
 *
 * This macro tries to unlock the given mutex. If the unlock operation fails,
 * it logs an error and returns an error code.
 *
 * @param mutex The mutex to unlock.
 */
#define MUTEX_UNLOCK(mutex)                                           \
    do                                                                \
    {                                                                 \
        if (pthread_mutex_unlock(mutex) != 0)                         \
        {                                                             \
            LOG_ERROR(ERROR_THREAD_UNLOCK, "Failed to unlock mutex"); \
            return ERROR_THREAD_UNLOCK;                               \
        }                                                             \
    } while (0)

#define RETURN_IF_ERROR(expr)    \
    do                           \
    {                            \
        ErrorCode ec = (expr);   \
        if (ec != ERROR_SUCCESS) \
            return ec;           \
    } while (0)

#endif // LIMDY_UTILS_H