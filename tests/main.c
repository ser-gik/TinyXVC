/* SPDX-License-Identifier: BSD-2-Clause */

#include "ttest/test.h"

#include "txvc/log.h"

#include <stdlib.h>

int main(int argc, const char **argv) {
    (void) argc;
    (void) argv;
    txvc_log_configure("all+", LOG_LEVEL_ERROR,false);
    return ttest_run_all() ? EXIT_SUCCESS : EXIT_FAILURE;
}

