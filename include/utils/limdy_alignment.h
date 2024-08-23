#ifndef LIMDY_ALIGNMENT_H
#define LIMDY_ALIGNMENT_H

#include <stddef.h>
#include <stdalign.h>

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define LIMDY_MAX_ALIGN alignof(max_align_t)
#else
#define LIMDY_MAX_ALIGN (sizeof(long double) > sizeof(long long) ? sizeof(long double) : sizeof(long long))
#endif

#define LIMDY_ALIGN_UP(n, align) (((n) + (align) - 1) & ~((align) - 1))
#define LIMDY_ALIGN_DOWN(n, align) ((n) & ~((align) - 1))

#define LIMDY_ALIGNED_SIZE(size) LIMDY_ALIGN_UP(size, LIMDY_MAX_ALIGN)

#endif // LIMDY_ALIGNMENT_H