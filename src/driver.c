
#include "driver.h"

#include <stddef.h>

/* These symbols are defined in driver.ld */
extern const struct txvc_driver __txvc_drivers_begin[];
extern const struct txvc_driver __txvc_drivers_end[];

const struct txvc_driver* txvc_enumerate_drivers(
        bool (*fn)(const struct txvc_driver *d, const void *extra), const void *extra) {
    for (const struct txvc_driver* d = __txvc_drivers_begin; d != __txvc_drivers_end; d++) {
        if(!fn(d, extra)) {
            return d;
        }
    }
    return NULL;
}

