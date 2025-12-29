#ifndef FIXED_H
#define FIXED_H

#include <stdint.h>

// ============ FIXED-POINT MATH ============
// 16.16 format for network determinism

typedef int32_t fixed16_t;

#define FIXED_SHIFT 16
#define FIXED_ONE (1 << FIXED_SHIFT)
#define FIXED_HALF (1 << (FIXED_SHIFT - 1))

// Conversion macros
#define FLOAT_TO_FIXED(f) ((fixed16_t)((f) * FIXED_ONE))
#define FIXED_TO_FLOAT(f) ((float)(f) / FIXED_ONE)
#define INT_TO_FIXED(i) ((fixed16_t)((i) << FIXED_SHIFT))
#define FIXED_TO_INT(f) ((int)((f) >> FIXED_SHIFT))

// Fixed-point arithmetic with rounding (prevents numerical drift)
static inline fixed16_t fixed_mul(fixed16_t a, fixed16_t b) {
    int64_t result = (int64_t)a * b;
    // Round to nearest instead of truncating (add half before shift)
    if (result >= 0) {
        return (fixed16_t)((result + FIXED_HALF) >> FIXED_SHIFT);
    } else {
        return (fixed16_t)((result - FIXED_HALF) >> FIXED_SHIFT);
    }
}

static inline fixed16_t fixed_div(fixed16_t a, fixed16_t b) {
    int64_t result = ((int64_t)a << FIXED_SHIFT);
    // Round to nearest
    if ((result >= 0) == (b >= 0)) {
        return (fixed16_t)((result + b/2) / b);
    } else {
        return (fixed16_t)((result - b/2) / b);
    }
}

#endif // FIXED_H
