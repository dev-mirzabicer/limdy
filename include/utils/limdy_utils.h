#ifndef LIMDY_UTILS_H
#define LIMDY_UTILS_H

#include "error_handler.h"
#include <pthread.h>

#define CHECK_NULL(ptr, error_code)                \
    do                                             \
    {                                              \
        if ((ptr) == NULL)                         \
        {                                          \
            LOG_ERROR(error_code, "Null pointer"); \
            return error_code;                     \
        }                                          \
    } while (0)

#define MUTEX_LOCK(mutex)                                         \
    do                                                            \
    {                                                             \
        if (pthread_mutex_lock(mutex) != 0)                       \
        {                                                         \
            LOG_ERROR(ERROR_THREAD_LOCK, "Failed to lock mutex"); \
            return ERROR_THREAD_LOCK;                             \
        }                                                         \
    } while (0)

#define MUTEX_UNLOCK(mutex)                                           \
    do                                                                \
    {                                                                 \
        if (pthread_mutex_unlock(mutex) != 0)                         \
        {                                                             \
            LOG_ERROR(ERROR_THREAD_UNLOCK, "Failed to unlock mutex"); \
            return ERROR_THREAD_UNLOCK;                               \
        }                                                             \
    } while (0)

// Add any other utility macros here

#endif // LIMDY_UTILS_H