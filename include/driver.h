
#pragma once

#include <stdbool.h>
#include <stdint.h>

struct txvc_driver {
    const char *name;
    const char *help;

    bool (*activate)(const char **argNames, const char **argValues);
    bool (*deactivate)(void);

    int (*max_vector_bits)(void);
    int (*set_tck_period)(int tckPeriodNs);
    bool (*shift_bits)(int numBits,
            const uint8_t *tmsVector,
            const uint8_t *tdiVector,
            uint8_t *tdoVector
            );
} __attribute__((aligned(16)));

#define TXVC_DRIVER(name) \
    static struct txvc_driver name \
    __attribute_used__ \
    __attribute__((aligned(16))) \
    __attribute__((section(".txvc_drivers")))

extern const struct txvc_driver* txvc_enumerate_drivers(
        bool (*fn)(const struct txvc_driver *d, const void *extra), const void *extra);

