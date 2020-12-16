
#include "module.h"

#include <stddef.h>

/* These symbols are defined in module.ld */
extern const struct txvc_module __txvc_modules_begin[];
extern const struct txvc_module __txvc_modules_end[];

const struct txvc_module* txvc_enumerate_modules(
        bool (*fn)(const struct txvc_module *m, const void *extra), const void *extra) {
    for (const struct txvc_module* m = __txvc_modules_begin; m != __txvc_modules_end; m++) {
        if(!fn(m, extra)) {
            return m;
        }
    }
    return NULL;
}

